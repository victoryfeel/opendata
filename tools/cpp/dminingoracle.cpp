#include "_ooci.h"
#include "_public.h"
#include <cstring>
using namespace idc;

void app_exit(int sig);
void app_help();
clogfile logfile;
cpactive pactive;
connection conn;

struct st_arg {
  char connstr[101];
  char charset[51];
  char selectsql[1024]; // 从数据源数据库抽取数据的SQL语句。
  char fieldstr[501];   // 抽取数据的SQL语句输出结果集字段名，字段名之间用逗号分隔。
  char fieldlen[501];   // 抽取数据的SQL语句输出结果集字段的长度，用逗号分隔。
  char bfilename[31];   // 输出xml文件的前缀。
  char efilename[31];   // 输出xml文件的后缀。
  char outpath[256];    // 输出xml文件存放的目录。
  int maxcount;
  char starttime[52]; // 程序运行的时间区间,什么时候抽取
  char incfield[31];  // 递增字段名,全量(0) or 增量(keyid)
  char incfilename[256];
  char connstr1[101];
  int timeout;
  char pname[51];
} starg;

ccmdstr fieldname; // 结果集字段名数组(分割后)
ccmdstr fieldlen;  // 结果集字段长度数组(分割后)
long max_increase_value;
int incfieldpos = -1;

bool _xmltoarg(const char* strxmlbuffer);
bool _dminingoracle();
bool instarttime();
bool read_max_increase_field();
bool write_max_increase_field();


int main(int argc, char* argv[]) {
  /////////////////////////////////////////////////////////////////////
  //// 0-初始化程序
  /////////////////////////////////////////////////////////////////////
  if (argc != 3) {
    app_help();
    return -1;
  }
  signal(SIGINT, app_exit);
  signal(SIGTERM, app_exit);
  if (logfile.open(argv[1]) == false) {
    printf("logfile.open(%s) failed.\n", argv[1]);
    return -1;
  }
  // 解析程序参数
  if (_xmltoarg(argv[2]) == false) {
    logfile.write("_xmltoarg(%s) failed.\n", argv[2]);
    app_exit(-1);
  }
  /////////////////////////////////////////////////////////////////////
  //// 1-判断当前时间是否在允许运行的时间区间内
  /////////////////////////////////////////////////////////////////////
  if (instarttime() == false) {
    return 0;
  }
  pactive.addpinfo(starg.timeout, starg.pname);
  /////////////////////////////////////////////////////////////////////
  //// 2-连接数据源的数据库
  /////////////////////////////////////////////////////////////////////
  if (conn.connecttodb(starg.connstr, starg.charset) != 0) {
    logfile.write("conn.connecttodb(%s) failed.\n%s\n", starg.connstr, conn.message());
    app_exit(-1);
  }
  logfile.write("connect database(%s) sucess. \n", starg.connstr);
  /////////////////////////////////////////////////////////////////////
  //// 3-从文件/数据库读取上次的增量抽取位置
  /////////////////////////////////////////////////////////////////////
  if (read_max_increase_field() == false) {
    app_exit(-1);
  }
  /////////////////////////////////////////////////////////////////////
  //// 4-调用_dminingoracle()执行数据抽取
  /////////////////////////////////////////////////////////////////////
  _dminingoracle();

  return 0;
}

/**
 * @brief {保存增量抽取位置}
 * @details:
    1. 如果不是增量抽取，直接返回
    2. 如果配置了connstr1：
      ├─ 连接数据库
      ├─ UPDATE T_MAXINCVALUE表
      ├─ 如果表不存在（ORA-00942），创建表并INSERT
      └─ 如果没有记录，INSERT新记录
    3. 否则写入到incfilename文件
 * @return true
 * @return false
 * @author Alex Mak (aliasgmail@duck.com)
 * @date 2026-06-01
 */
bool write_max_increase_field() {
  if (strlen(starg.incfield) == 0) {
    return true;
  }
  if (strlen(starg.connstr1) != 0) {
    connection conn1;
    if (conn1.connecttodb(starg.connstr1, starg.charset) != 0) {
      logfile.write("connect database(%s) failed.\n%s\n", starg.connstr1, conn1.message());
      return false;
    }
    sqlstatement stmt(&conn1);
    stmt.prepare("update T_MAXINCVALUE set maxincvalue=:1 where pname=:2");
    stmt.bindin(1, max_increase_value);
    stmt.bindin(2, starg.pname);
    if (stmt.execute() != 0) {
      // ORA-00942错误表示表不存在, 创建新表
      if (stmt.rc() == 942) {
        conn1.execute(
          "create table T_MAXINCVALUE(pname varchar2(50),maxincvalue number(15),primary "
          "key(pname))"
        );
        conn1.execute(
          "insert into T_MAXINCVALUE values('%s',%ld)", starg.pname, max_increase_value
        );
        conn1.commit();
        return true;
      } else {
        logfile.write("stmt.execute()failed.\n%s\n%s\n", stmt.sql(), stmt.message());
        return false;
      }
    } else {
      // 表中无记录，是空表，则插入记录
      if (stmt.rpc() == 0) {
        conn1.execute(
          "insert into T_MAXINCVALUE values('%s',%ld)", starg.pname, max_increase_value
        );
      }
      conn1.commit();
    }
  } else {
    cofile ofile;
    if (ofile.open(starg.incfilename, false) == false) {
      logfile.write("ofile.open(%s) failed.\n", starg.incfilename);
      return false;
    }
    ofile.writeline("%ld", max_increase_value);
  }

  return true;
}

/**
 * @brief {读取增量抽取位置}
 * @details:
      1. 初始化imaxincvalue=0
      2. 如果不是增量抽取（incfield为空），直接返回
      3. 查找incfield在fieldname数组中的位置
      4. 从connstr1数据库的T_MAXINCVALUE表读取，或从incfilename文件读取上次的最大值
      5. 记录日志
 * @return true
 * @return false
 * @author Alex Mak (aliasgmail@duck.com)
 * @date 2026-06-01
 */
bool read_max_increase_field() {
  max_increase_value = 0;
  if (strlen(starg.incfield) == 0) {
    return true;
  }
  // 查找incfield递增字段，在fieldname结果集字段名数组中的位置
  for (int i = 0; i < fieldname.size(); i++) {
    if (fieldname[i] == starg.incfield) {
      incfieldpos = i;
      break;
    }
  }
  if (incfieldpos == -1) {
    logfile.write("递增字段名%s不在列表%s中. \n", starg.incfield, starg.fieldstr);
  }

  // 从connstr1数据库的T_MAXINCVALUE表读取，或从incfilename文件读取上次的最大值
  if (strlen(starg.connstr1) != 0) {
    connection conn1;
    if (conn1.connecttodb(starg.connstr1, starg.charset) != 0) {
      logfile.write("connect database(%s) failed.\n%s\n", starg.connstr1, conn1.message());
      return false;
    }
    sqlstatement stmt(&conn1);
    stmt.prepare("select maxincvalue from T_MAXINCVALUE where pname =:1");
    stmt.bindin(1, starg.pname);
    stmt.bindout(1, max_increase_value);
    stmt.execute();
    stmt.next();
  } else {
    cifile ifile;
    // 打开失败原因：1.第一次运行，return true; 2. 文件丢失，要重新抽取
    // 因此重要数据都是用数据库表存放，文件容易丢失
    if (ifile.open(starg.incfilename) == false) {
      return true;
    }
    string tmp_store_maxvalue;
    ifile.readline(tmp_store_maxvalue);
    max_increase_value = atoi(tmp_store_maxvalue.c_str());
  }
  logfile.write("上次抽取数据的位置(%s=%ld).\n", starg.incfield, max_increase_value);

  return true;
}


bool _dminingoracle() {
  /////////////////////////////////////////////////////////////////////
  //// 1-准备sql语句,绑定结果集变量(字段名与长度)到字符串数组
  /////////////////////////////////////////////////////////////////////
  sqlstatement stmt(&conn);
  stmt.prepare("%s", starg.selectsql);
  string str_field_value[fieldname.size()];
  for (int i = 1; i <= fieldname.size(); i++) {
    stmt.bindout(i, str_field_value[i - 1], stoi(fieldlen[i - 1]));
  }
  /////////////////////////////////////////////////////////////////////
  //// 2-如果是增量抽取，绑定输入参数上次抽取字段最大值max_increase_value
  /////////////////////////////////////////////////////////////////////
  if (strlen(starg.incfield) != 0) {
    stmt.bindin(1, max_increase_value);
  }
  /////////////////////////////////////////////////////////////////////
  ///// 3-执行SQL语句
  /////////////////////////////////////////////////////////////////////
  if (stmt.execute() != 0) {
    logfile.write("stmt.execute() failed.\n%s\n%s\n", stmt.sql(), stmt.message());
    return false;
  }

  pactive.uptatime();

  /////////////////////////////////////////////////////////////////////
  //// 4-逐行读取结果集
  /////////////////////////////////////////////////////////////////////
  string xml_filename;
  cofile ofile;
  int iseq = 1; // xml file name number

  while (true) {
    if (stmt.next() != 0) {
      break;
    }
    // 4.1-判断是否需要打开新的XML文件,满足maxcount时则打开新xml文件
    if (ofile.isopen() == false) {
      sformat(
        xml_filename,
        "%s/%s_%s_%s_%d.xml",
        starg.outpath,
        starg.bfilename,
        ltime1("yyyymmddhh24miss").c_str(),
        starg.efilename,
        iseq++
      );
      if (ofile.open(xml_filename) == false) {
        logfile.write("ofile.open(%s) failed.\n", xml_filename.c_str());
        return false;
      }
      // xml文件开始标签
      ofile.writeline("%s", "<data>\n");
    }

    // 4.2-写入字段数据到XML（<fieldname>value</fieldname>格式）
    for (int i = 1; i <= fieldname.size(); i++) {
      ofile.writeline(
        "<%s>%s</%s>",
        fieldname[i - 1].c_str(),
        str_field_value[i - 1].c_str(),
        fieldname[i - 1].c_str()
      );
    }
    // 行结束标签
    ofile.writeline("%s", "<endl/>\n");

    // 4.3-当记写入录数达到maxcount时，关闭XML文件并重命名
    if (starg.maxcount > 0 && (stmt.rpc() % starg.maxcount == 0)) {
      ofile.writeline("%s", "</data>\n");
      if (ofile.closeandrename() == false) {
        logfile.write("ofile.closeandrename(%s) failed.\n", xml_filename.c_str());
        return false;
      }
      logfile.write("生成文件%s(%d). \n", xml_filename.c_str(), starg.maxcount);

      pactive.uptatime();
    }

    // 4.4-更新递增字段keyid最大值
    if (
      (strlen(starg.incfield) != 0) && (max_increase_value < stol(str_field_value[incfieldpos]))
    ) {
      max_increase_value = stol(str_field_value[incfieldpos]);
    }
  }

  /////////////////////////////////////////////////////////////////////
  ///// 5-处理最后一个XML文件（可能不足maxcount条记录）
  /////////////////////////////////////////////////////////////////////
  if (ofile.isopen() == true) {
    ofile.writeline("%s", "</data>\n");
    if (ofile.closeandrename() == false) {
      logfile.write("ofile.closeandrename(%s) failed.\n", xml_filename.c_str());
      return false;
    }
    if (starg.maxcount == 0) {
      logfile.write("生成文件%s(%d). \n", xml_filename.c_str(), stmt.rpc());
    } else {
      logfile.write("生成文件%s(%d). \n", xml_filename.c_str(), stmt.rpc() % starg.maxcount);
    }
  }

  /////////////////////////////////////////////////////////////////////
  //// 6-如果有数据被抽取，保存增量抽取的最大值到数据库或文件中
  //// 编写write_max_increase_field()实现
  /////////////////////////////////////////////////////////////////////
  if (stmt.rpc() > 0) {
    write_max_increase_field();
  }

  return true;
}


bool instarttime() {
  if (strlen(starg.starttime) != 0) {
    string strhh24 = ltime1("hh24");
    if (strstr(starg.starttime, strhh24.c_str()) == nullptr) {
      return false;
    }
  }
  return true;
}


bool _xmltoarg(const char* strxmlbuffer) {
  memset(&starg, 0, sizeof(struct st_arg));
  getxmlbuffer(strxmlbuffer, "connstr", starg.connstr, 100); // 数据源数据库的连接参数。
  if (strlen(starg.connstr) == 0) {
    logfile.write("%s", "connstr is null.\n");
    return false;
  }
  getxmlbuffer(strxmlbuffer, "charset", starg.charset, 50); // 数据库的字符集。
  if (strlen(starg.charset) == 0) {
    logfile.write("%s", "charset is null.\n");
    return false;
  }
  getxmlbuffer(
    strxmlbuffer, "selectsql", starg.selectsql, 1000
  ); // 从数据源数据库抽取数据的SQL语句。
  if (strlen(starg.selectsql) == 0) {
    logfile.write("%s", "selectsql is null.\n");
    return false;
  }
  getxmlbuffer(strxmlbuffer, "fieldstr", starg.fieldstr, 500); // 结果集字段名列表。
  if (strlen(starg.fieldstr) == 0) {
    logfile.write("%s", "fieldstr is null.\n");
    return false;
  }
  getxmlbuffer(strxmlbuffer, "fieldlen", starg.fieldlen, 500); // 结果集字段长度列表。
  if (strlen(starg.fieldlen) == 0) {
    logfile.write("%s", "fieldlen is null.\n");
    return false;
  }
  getxmlbuffer(strxmlbuffer, "bfilename", starg.bfilename, 30); // 输出xml文件存放的目录。
  if (strlen(starg.bfilename) == 0) {
    logfile.write("%s", "bfilename is null.\n");
    return false;
  }
  getxmlbuffer(strxmlbuffer, "efilename", starg.efilename, 30); // 输出xml文件的前缀。
  if (strlen(starg.efilename) == 0) {
    logfile.write("%s", "efilename is null.\n");
    return false;
  }
  getxmlbuffer(strxmlbuffer, "outpath", starg.outpath, 255); // 输出xml文件的后缀。
  if (strlen(starg.outpath) == 0) {
    logfile.write("%s", "outpath is null.\n");
    return false;
  }

  getxmlbuffer(strxmlbuffer, "maxcount", starg.maxcount); // 输出xml文件的最大记录数，可选参数。
  getxmlbuffer(strxmlbuffer, "starttime", starg.starttime, 50); // 程序运行的时间区间，可选参数。
  getxmlbuffer(strxmlbuffer, "incfield", starg.incfield, 30);   // 递增字段名，可选参数。
  getxmlbuffer(
    strxmlbuffer, "incfilename", starg.incfilename, 255
  ); // 已抽取数据的递增字段最大值存放的文件，可选参数。
  getxmlbuffer(
    strxmlbuffer, "connstr1", starg.connstr1, 100
  ); // 已抽取数据的递增字段最大值存放的数据库的连接参数，可选参数。
  getxmlbuffer(strxmlbuffer, "timeout", starg.timeout); // 进程心跳的超时时间。
  if (starg.timeout == 0) {
    logfile.write("%s", "timeout is null.\n");
    return false;
  }
  getxmlbuffer(strxmlbuffer, "pname", starg.pname, 50); // 进程名。
  if (strlen(starg.pname) == 0) {
    logfile.write("%s", "pname is null.\n");
    return false;
  }

  // 拆分starg.fieldstr到fieldname中。
  fieldname.splittocmd(starg.fieldstr, ",");
  // 拆分starg.fieldlen到fieldlen中。
  fieldlen.splittocmd(starg.fieldlen, ",");
  // 判断fieldname和fieldlen两个数组的大小是否相同。
  if (fieldlen.size() != fieldname.size()) {
    logfile.write("%s", "fieldstr和fieldlen的元素个数不一致。\n");
    return false;
  }

  // 如果是增量抽取，incfilename和connstr1必二选一。
  if (strlen(starg.incfield) > 0) {
    if ((strlen(starg.incfilename) == 0) && (strlen(starg.connstr1) == 0)) {
      logfile.write("%s", "如果是增量抽取，incfilename和connstr1必二选一，不能都为空。\n");
      return false;
    }
  }

  return true;
}


void app_exit(int sig) {
  logfile.write("程序退出，sig=%d\n\n", sig);
  exit(0);
}


void app_help() {
  printf("Using:/project/tools/bin/dminingoracle logfilename xmlbuffer\n\n");
  printf(
    "Sample:/project/tools/bin/procctl 3600 /project/tools/bin/dminingoracle "
    "/log/idc/dminingoracle_ZHOBTCODE.log "
    "\"<connstr>idc/idcpwd@snorcl11g_128</connstr><charset>Simplified "
    "Chinese_China.AL32UTF8</charset>"
    "<selectsql>select obtid,cityname,provname,lat,lon,height from T_ZHOBTCODE where obtid like "
    "'5%%%%'</selectsql>"
    "<fieldstr>obtid,cityname,provname,lat,lon,height</fieldstr><fieldlen>5,30,30,10,10,10</"
    "fieldlen>"
    "<bfilename>ZHOBTCODE</bfilename><efilename>togxpt</efilename><outpath>/idcdata/dmindata</"
    "outpath>"
    "<timeout>30</timeout><pname>dminingoracle_ZHOBTCODE</pname>\"\n\n"
  );
  printf(
    "       /project/tools/bin/procctl   30 /project/tools/bin/dminingoracle "
    "/log/idc/dminingoracle_ZHOBTMIND.log "
    "\"<connstr>idc/idcpwd@snorcl11g_128</connstr><charset>Simplified "
    "Chinese_China.AL32UTF8</charset>"
    "<selectsql>select obtid,to_char(ddatetime,'yyyymmddhh24miss'),t,p,u,wd,wf,r,vis,keyid from "
    "T_ZHOBTMIND where keyid>:1 and obtid like '5%%%%'</selectsql>"
    "<fieldstr>obtid,ddatetime,t,p,u,wd,wf,r,vis,keyid</fieldstr><fieldlen>5,19,8,8,8,8,8,8,8,15</"
    "fieldlen>"
    "<bfilename>ZHOBTMIND</bfilename><efilename>togxpt</efilename><outpath>/idcdata/dmindata</"
    "outpath>"
    "<starttime></starttime><incfield>keyid</incfield>"
    "<incfilename>/idcdata/dmining/dminingoracle_ZHOBTMIND_togxpt.keyid</incfilename>"
    "<timeout>30</timeout><pname>dminingoracle_ZHOBTMIND_togxpt</pname>"
    "<maxcount>1000</maxcount><connstr1>scott/tiger@snorcl11g_128</connstr1>\"\n\n"
  );

  printf("本程序是共享平台的公共功能模块，用于从Oracle数据库源表抽取数据，生成xml文件。\n");
  printf("logfilename 本程序运行的日志文件。\n");
  printf("xmlbuffer   本程序运行的参数，用xml表示，具体如下：\n\n");
  printf("connstr     数据源数据库的连接参数，格式：username/passwd@tnsname。\n");
  printf(
    "charset     数据库的字符集，这个参数要与数据源数据库保持一致，否则会出现中文乱码的情况。\n"
  );
  printf(
    "selectsql   "
    "从数据源数据库抽取数据的SQL语句，如果是增量抽取，一定要用递增字段作为查询条件，如where "
    "keyid>:1。全量抽取使用静态SQL,增量抽取使用动态SQL。\n"
  );
  printf(
    "fieldstr    "
    "抽取数据的SQL语句输出结果集的字段名列表，中间用逗号分隔，将作为xml文件的标签字段名。\n"
  );
  printf(
    "fieldlen    "
    "抽取数据的SQL语句输出结果集字段的长度列表，中间用逗号分隔。fieldstr与fieldlen的字段必须一一对"
    "应。\n"
  );
  printf("outpath     输出xml文件存放的目录。\n");
  printf("bfilename   输出xml文件的前缀。如ZHOBTMIND_\n");
  printf("efilename   输出xml文件的后缀。如ZHOBTMIND_time_togxpt_num.xml\n");
  printf(
    "maxcount    "
    "输出xml文件的最大记录数，缺省是0，表示无限制，如果本参数取值为0，注意适当加大timeout的取值，防"
    "止程序超时。最多一万行，防止数据库大事务产生。\n"
  );
  printf(
    "starttime   "
    "程序运行的时间区间，例如02,13表示：如果程序启动时，踏中02时和13时则运行，其它时间不运行。"
    "如果starttime为空，表示不启用，只要本程序启动，就会执行数据抽取任务，为了减少数据源数据库压力"
    "抽取数据的时候，如果对时效性没有要求，一般在数据源数据库空闲的时候时进行。\n"
  );
  printf(
    "incfield    递增字段名，它必须是fieldstr中的字段名，并且只能是整型，一般为自增字段如keyid。"
    "如果incfield为空，表示不采用增量抽取的方案。"
  );
  printf(
    "incfilename 已抽取数据的递增字段最大值存放的文件，如果该文件丢失，将重新抽取全部的数据。\n"
  );
  printf(
    "connstr1    "
    "已抽取数据的递增字段最大值存放的数据库的连接参数。connstr1和incfilename二选一，connstr1优先。"
  );
  printf("timeout     本程序的超时时间，单位：秒。\n");
  printf("pname       进程名，尽可能采用易懂的、与其它进程不同的名称，方便故障排查。\n\n\n");
}
