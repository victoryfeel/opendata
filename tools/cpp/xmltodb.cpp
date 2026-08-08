#include <string>

#include "_ooci.h"
#include "_public.h"
#include "_tools.h"
using namespace idc;

clogfile logfile;
connection conn;
cpactive pactive;

struct st_arg {
  char connstr[101];
  char charset[51];
  char inifilename[301]; // 数据入库的参数配置文件。
  char xmlpath[301];     // 待入库xml文件存放的目录。
  char xmlpathbak[301];  // xml文件入库后的备份目录。
  char xmlpatherr[301];  // 入库失败的xml文件存放的目录。
  int timetvl;           // 本程序运行的时间间隔，本程序常驻内存。
  int timeout;
  char pname[51];
} starg;
void app_help(char* argv[]);
void app_exit(int sig);
bool xml_to_arg(const char* xmlbuffer);

ctimer timer; // 处理一个xml文件的时间
int totalcount, insertcount, updatecount;

struct st_xmltotable {
  char xml_file_name[301]; // xml文件名
  char table_name[31];     // 目标库表名
  int update_flag;         // 更新标志，1-更新，2-不更新
  char execute_sql[301];   // 处理xml前执行的sql语句
} stxmltotable;
vector<struct st_xmltotable> vxmltotable;
TableColumn table_column;
string insert_sql;
string update_sql;

bool load_xml_to_table();
bool find_xml_to_table(const string& xml_file_name);
bool _xmltodb();
int _xmltodb(const string& full_file_name, const string& xml_file_name);
void make_sql();
// xml每一行解析出来的字段值,用用于Insert/Update sql绑定变量
vector<string> column_value;
sqlstatement stmt_update, stmt_insert;
void prepare_sql();
bool execsql();
// 解析出来的值放在column_value容器中
void split_value_buffer(const string& strbuffer);
bool xmltobakerr(const string& fullfilename, const string& srcpath, const string& dstpath);


int main(int argc, char* argv[]) {
  if (argc != 3) {
    app_help(argv);
    return -1;
  }
  // closeioandsignal(true);
  signal(SIGINT, app_exit);
  signal(SIGTERM, app_exit);
  if (logfile.open(argv[1]) == false) {
    printf("logfile.open(%s) failed.\n", argv[1]);
    return -1;
  }
  if (xml_to_arg(argv[2]) == false) {
    return -1;
  }
  pactive.addpinfo(starg.timeout, starg.pname);
  _xmltodb();

  return 0;
}


bool xmltobakerr(const string& fullfilename, const string& srcpath, const string& dstpath) {
  string dstfilename = fullfilename;
  replacestr(dstfilename, srcpath, dstpath, false);
  if (renamefile(fullfilename, dstfilename.c_str()) == false) {
    logfile.write("renamefile(%s,%s) failed.\n", fullfilename.c_str(), dstfilename.c_str());
  }

  return true;
}


bool execsql() {
  if (strlen(stxmltotable.execute_sql) == 0) {
    return true;
  }
  sqlstatement stmt(&conn);
  stmt.prepare(stxmltotable.execute_sql);
  if (stmt.execute() != 0) {
    logfile.write("stmt.execute() failed.\n%s\n%s\n", stmt.sql(), stmt.message());
    return false;
  }

  return true;
}


void prepare_sql() {
  column_value.resize(table_column.m_column_info.size());
  // 1. prepare insert sql
  stmt_insert.connect(&conn);
  stmt_insert.prepare(insert_sql);

  // logfile.write("\n%s\n", stmt_insert.sql());
  int value_seq = 1;
  for (int i = 0; i < table_column.m_column_info.size(); i++) {
    // upttime, keyid这两个字段不需要绑定参数
    if (
      (strcmp(table_column.m_column_info[i].column_name, "upttime") == 0) ||
      (strcmp(table_column.m_column_info[i].column_name, "keyid") == 0)
    ) {
      continue;
    }
    stmt_insert.bindin(value_seq, column_value[i], table_column.m_column_info[i].column_length);
    // logfile.write(
    //   "stmt_insert.bindin(%d,column_value[%d],%d);\n",
    //   value_seq,
    //   i,
    //   table_column.m_column_info[i].column_length
    // );

    value_seq++;
  }

  // 2. prepare update sql
  if (stxmltotable.update_flag != 1) {
    return;
  }
  stmt_update.connect(&conn);
  stmt_update.prepare(update_sql);
  // logfile.write("\n%s\n", stmt_update.sql());
  value_seq = 1;
  // set部分的参数
  for (int i = 0; i < table_column.m_column_info.size(); i++) {
    // 主键字段不需要绑定在set后面, 不需要处理upttime, keyid两个字段
    if (table_column.m_column_info[i].primary_key_sequence != 0) {
      continue;
    }
    if (
      (strcmp(table_column.m_column_info[i].column_name, "upttime") == 0) ||
      (strcmp(table_column.m_column_info[i].column_name, "keyid") == 0)
    ) {
      continue;
    }
    stmt_update.bindin(value_seq, column_value[i], table_column.m_column_info[i].column_length);
    // logfile.write(
    //   "stmt_update.bindin(%d,column_value[%d],%d);\n",
    //   value_seq,
    //   i,
    //   table_column.m_column_info[i].column_length
    // );
    value_seq++;
  }

  // where部分的参数绑定
  for (int i = 0; i < table_column.m_column_info.size(); i++) {

    if (table_column.m_column_info[i].primary_key_sequence == 0) {
      continue;
    }
    stmt_update.bindin(value_seq, column_value[i], table_column.m_column_info[i].column_length);
    // logfile.write(
    //   "stmt_update.bindin(%d,column_value[%d],%d);\n",
    //   value_seq,
    //   i,
    //   table_column.m_column_info[i].column_length
    // );
    value_seq++;
  }


  return;
}


void make_sql() {
  // 1. 拼接insert_sql
  string insert_columns;
  string insert_values;
  int value_seq = 1;
  for (auto& i : table_column.m_column_info) {
    if (strcmp(i.column_name, "upttime") == 0) {
      continue;
    }
    insert_columns = insert_columns + i.column_name + ",";

    // value部分要区分keyid, date, 非date字段
    // keyid在与表同名的序列器中获取，T_ZHOBTCODE, 表名+2获得ZHOBTCODE
    // 日期使用todate()处理
    if (strcmp(i.column_name, "keyid") == 0) {
      insert_values = insert_values + sformat("SEQ_%s.nextval", stxmltotable.table_name + 2) + ",";
    } else {
      if (strcmp(i.data_type, "date") == 0) {
        insert_values = insert_values + sformat("to_date(:%d,'yyyymmddhh24miss')", value_seq) + ",";
      } else {
        insert_values = insert_values + sformat(":%d", value_seq) + ",";
      }
      value_seq++;
    }
  }
  // 删除多余","
  deleterchr(insert_columns, ',');
  deleterchr(insert_values, ',');

  insert_sql = sformat(
    "insert into %s (%s) values (%s)",
    stxmltotable.table_name,
    insert_columns.c_str(),
    insert_values.c_str()
  );

  // logfile.write("insert_sql:\n%s\n", insert_sql.c_str());


  // 2. 拼接update_sql
  // update T_ZHOBTMIND1 set t=:1,p=:2,u=:3,wd=:4,wf=:5,r=:6,vis=:7
  // where obtid=:8 and ddatetime=to_date(:9,'yyyymmddhh24miss')
  if (stxmltotable.update_flag != 1) {
    return;
  }

  // 1) part1: 开头部分
  update_sql = sformat("update %s set ", stxmltotable.table_name);

  // 2) part2: set后面
  value_seq = 1;
  for (auto& i : table_column.m_column_info) {
    // 主键字段不需要在拼接在set后后面
    if (i.primary_key_sequence != 0) {
      continue;
    }
    // keyid不需要更新
    if (strcmp(i.column_name, "keyid") == 0) {
      continue;
    }
    // upttime字段使用sysdate赋值
    if (strcmp(i.column_name, "upttime") == 0) {
      update_sql = update_sql + "upttime=sysdate" + ",";
      continue;
    }
    // 其他字段拼接在set后面,区分date与非date
    if (strcmp(i.data_type, "date") != 0) {
      update_sql = update_sql + sformat("%s=:%d,", i.column_name, value_seq);
    } else {
      update_sql =
        update_sql + sformat("%s=to_date(:%d,'yyyymmddhh24miss')", i.column_name, value_seq);
    }
    value_seq++;
  }
  deleterchr(update_sql, ',');

  // 3) part3: where后面
  // update T_ZHOBTMIND1 set t=:1,p=:2,u=:3,wd=:4,wf=:5,r=:6,vis=:7
  // where obtid=:8 and ddatetime=to_date(:9,'yyyymmddhh24miss')
  update_sql = update_sql + " where 1=1 "; // 为了后面and条件关键字拼接
  for (auto& i : table_column.m_column_info) {
    if (i.primary_key_sequence == 0) {
      continue;
    }
    if (strcmp(i.data_type, "date") != 0) {
      update_sql = update_sql + sformat(" and %s=:%d", i.column_name, value_seq);
    } else {
      update_sql =
        update_sql + sformat(" and %s=to_date(:%d,'yyyymmddhh24miss')", i.column_name, value_seq);
    }
    value_seq++;
  }

  // logfile.write("update_sql:\n%s\n", update_sql.c_str());

  return;
}


int _xmltodb(const string& full_file_name, const string& xml_file_name) {
  timer.start();
  totalcount = insertcount = updatecount = 0;


  // 根据xml文件名找到入库的表名
  if (find_xml_to_table(xml_file_name) == false) {
    return 1;
  }
  // 获取表的列信息和主键信息
  if (table_column.get_column_info(conn, stxmltotable.table_name) == false) {
    return 2;
  }
  if (table_column.get_primary_key_info(conn, stxmltotable.table_name) == false) {
    return 2;
  }
  // 待入库表不存在
  if (table_column.m_all_columns.size() == 0) {
    return 3;
  }

  // 根据表的字段名和主键，拼接插入和更新表的SQL语句
  make_sql();
  prepare_sql();
  // 打开xml文件，读取一行数据，解析成字段值，执行入库的SQL语句
  // 1. 先执行xml参数中的sql语句
  if (execsql() == false) {
    return 4;
  }
  cifile ifile;
  if (ifile.open(full_file_name) == false) {
    conn.rollback();
    return 5;
  }

  string strbuffer;
  while (true) {
    // 读取一行数据
    if (ifile.readline(strbuffer, "<endl/>") == false) {
      break;
    }

    totalcount++;

    // 解析字段值
    split_value_buffer(strbuffer);
    // 执行insert, update, sql语句
    if (stmt_insert.execute() != 0) {
      if (stmt_insert.rc() == 1) {
        if (stxmltotable.update_flag == 1) {
          if (stmt_update.execute() != 0) {
            // 如果update失败，记录出错的行和错误原因，函数不返回，继续处理数据，也就是说，不理这一行。
            // 失败原因主要是数据本身有问题，例如时间的格式不正确、数值不合法、数值太大。
            logfile.write("\n%s\n", strbuffer.c_str());
            logfile.write(
              "stmt_update.execute() failed.\n%s\n%s\n", stmt_update.sql(), stmt_update.message()
            );
          } else {
            updatecount++;
          }
        }
      } else {
        // 如果insert失败，记录出错的行和错误原因，函数不返回，继续处理数据，也就是说，不理这一行。
        // 失败原因主要是数据本身有问题，例如时间的格式不正确、数值不合法、数值太大。
        logfile.write("\n%s\n", strbuffer.c_str());
        logfile.write(
          "stmt_insert.execute() failed.\n%s\n%s\n", stmt_insert.sql(), stmt_insert.message()
        );
        // 如果是数据库系统出了问题，常见的问题如下，还可能有更多的错误，如果出现了，再加进来。
        // ORA-03113: 通信通道的文件结尾；ORA-03114: 未连接到ORACLE；ORA-03135:
        // 连接失去联系；ORA-16014：归档失败。
        if (
          (stmt_insert.rc() == 3113) || (stmt_insert.rc() == 3114) || (stmt_insert.rc() == 3135) ||
          (stmt_insert.rc() == 16014)
        ) {
          return 2;
        }
      }
    } else {
      insertcount++;
    }
  }

  // 提交事务
  conn.commit();

  return 0;
}

void split_value_buffer(const string& strbuffer) {
  string tmp_buffer;
  for (int i = 0; i < table_column.m_column_info.size(); i++) {
    // 用临时变量是为了防止调用移动构造和移动赋值函数改变column_value数组中string的内部地址。
    getxmlbuffer(
      strbuffer,
      table_column.m_column_info[i].column_name,
      tmp_buffer,
      table_column.m_column_info[i].column_length
    );
    // 日期字段只需提取数字
    if (strcmp(table_column.m_column_info[i].data_type, "date") == 0) {
      picknumber(tmp_buffer, tmp_buffer, false, false);
    } else if (strcmp(table_column.m_column_info[i].data_type, "number") == 0) {
      // nnumber字段提取数字，+-符号，圆点
      picknumber(tmp_buffer, tmp_buffer, true, true);
    }
    // char字段不需要任何处理
    // column_value[i] = tmp_buffer; // 不能采用这行代码，会调用移动赋值函数。
    column_value[i] = tmp_buffer.c_str();
  }
  return;
}


bool _xmltodb() {
  cdir dir;
  int counter = 50; // 设定50是为了第一次进入循环时加载参数
  while (true) {
    // 入库参数需要定时更新
    if (counter > 30) {
      if (load_xml_to_table() == false) {
        return false;
      }
      counter = 0;
    } else {
      counter++;
    }
    // 文件排序的目的是让先生成的文件先入库
    if (dir.opendir(starg.xmlpath, "*.xml", 10000, false, true) == false) {
      logfile.write("dir.open(%s) failed.\n", starg.xmlpath);
      return false;
    }

    // 有数据需要入库时再连接数据库
    if (conn.isopen() == false) {
      if (conn.connecttodb(starg.connstr, starg.charset) != 0) {
        logfile.write("connect database(%s) failed.\n%s\n", starg.connstr, conn.message());
        return false;
      }
      logfile.write("connect database(%s) ok.\n", starg.connstr);
    }

    while (true) {
      if (dir.readdir() == false) {
        break;
      }
      logfile.write("处理文件(%s)...", dir.m_ffilename.c_str());

      // 处理xml文件的子函数, return 0成功，其余都是失败
      int ret = _xmltodb(dir.m_ffilename, dir.m_filename);

      pactive.uptatime();

      // 处理错误情况
      // 0-成功，没有错误。把已入库的xml文件移动到备份目录。
      if (ret == 0) {
        logfile << "ok(" << stxmltotable.table_name << ",总数=" << totalcount
                << ",插入=" << insertcount << ",更新=" << updatecount << ",耗时=" << timer.elapsed()
                << "秒" << ").\n";
        if (xmltobakerr(dir.m_ffilename, starg.xmlpath, starg.xmlpathbak) == false) {
          return false;
        }
      }

      // 1-入库参数不正确；3-待入库的表不存在；4-执行入库前的SQL语句失败。把xml文件移动到错误目录。
      if (ret == 1 || ret == 3 || ret == 4) {
        if (ret == 1) {
          logfile.write("%s", "入库参数不正确。\n");
        }
        if (ret == 3) {
          logfile.write("%s", "待入库的表不存在。\n");
        }
        if (ret == 4) {
          logfile.write("%s", "执行入库前的SQL语句失败。\n");
        }
        if (xmltobakerr(dir.m_ffilename, starg.xmlpath, starg.xmlpatherr) == false) {
          return false;
        }
      }
      // 2-数据库错误，函数返回，程序将退出。
      if (ret == 2) {
        logfile.write("%s", "数据库错误。\n");
        return false;
      }
      // 5- 打开xml文件失败，函数返回，程序将退出。
      if (ret == 5) {
        logfile.write("%s", "打开xml文件失败。\n");
        return false;
      }
    }
    // 如果刚才处理了文件，表示不空闲，可能不断的有文件需要入库，就不sleep了。
    if (dir.size() == 0) {
      sleep(starg.timetvl);
    }
  }

  pactive.uptatime();

  return true;
}


bool find_xml_to_table(const string& xml_file_name) {
  for (auto& i : vxmltotable) {
    if (matchstr(xml_file_name, i.xml_file_name) == true) {
      stxmltotable = i;
      return true;
    }
  }
  return false;
}


bool load_xml_to_table() {
  vxmltotable.clear();
  cifile ifile;
  if (ifile.open(starg.inifilename) == false) {
    logfile.write("ifile.open(%s) failed.\n", starg.inifilename);
    return false;
  }

  string tmp_buffer;
  while (true) {
    if (ifile.readline(tmp_buffer, "<endl />") == false) {
      break;
    }
    memset(&stxmltotable, 0, sizeof(struct st_xmltotable));
    getxmlbuffer(tmp_buffer, "filename", stxmltotable.xml_file_name, 300);
    getxmlbuffer(tmp_buffer, "tname", stxmltotable.table_name, 30);
    getxmlbuffer(tmp_buffer, "uptbz", stxmltotable.update_flag);
    getxmlbuffer(tmp_buffer, "execsql", stxmltotable.execute_sql, 300);
    vxmltotable.push_back(stxmltotable);
  }
  logfile.write("load_xml_to_table(%s) ok.\n", starg.inifilename);

  return true;
}


bool xml_to_arg(const char* xmlbuffer) {
  memset(&starg, 0, sizeof(struct st_arg));
  getxmlbuffer(xmlbuffer, "connstr", starg.connstr, 100);
  if (strlen(starg.connstr) == 0) {
    logfile.write("%s", "connstr is null.\n");
    return false;
  }
  getxmlbuffer(xmlbuffer, "charset", starg.charset, 50);
  if (strlen(starg.charset) == 0) {
    logfile.write("%s", "charset is null.\n");
    return false;
  }
  getxmlbuffer(xmlbuffer, "inifilename", starg.inifilename, 300);
  if (strlen(starg.inifilename) == 0) {
    logfile.write("%s", "inifilename is null.\n");
    return false;
  }
  getxmlbuffer(xmlbuffer, "xmlpath", starg.xmlpath, 300);
  if (strlen(starg.xmlpath) == 0) {
    logfile.write("%s", "xmlpath is null.\n");
    return false;
  }
  getxmlbuffer(xmlbuffer, "xmlpathbak", starg.xmlpathbak, 300);
  if (strlen(starg.xmlpathbak) == 0) {
    logfile.write("%s", "xmlpathbak is null.\n");
    return false;
  }
  getxmlbuffer(xmlbuffer, "xmlpatherr", starg.xmlpatherr, 300);
  if (strlen(starg.xmlpatherr) == 0) {
    logfile.write("%s", "xmlpatherr is null.\n");
    return false;
  }
  getxmlbuffer(xmlbuffer, "timetvl", starg.timetvl);
  if (starg.timetvl < 2) {
    starg.timetvl = 2;
  }
  if (starg.timetvl > 30) {
    starg.timetvl = 30;
  }

  getxmlbuffer(xmlbuffer, "timeout", starg.timeout);
  if (starg.timeout == 0) {
    logfile.write("%s", "timeout is null.\n");
    return false;
  }
  getxmlbuffer(xmlbuffer, "pname", starg.pname, 50);
  if (strlen(starg.pname) == 0) {
    logfile.write("%s", "pname is null.\n");
    return false;
  }

  return true;
}

void app_help(char* argv[]) {
  printf("Using:/project/tools/bin/xmltodb logfilename xmlbuffer\n\n");

  printf(
    "Sample:/project/tools/bin/procctl 10 /project/tools/bin/xmltodb /log/idc/xmltodb_vip.log "
    "\"<connstr>idc/idcpwd@snorcl11g_128</connstr><charset>Simplified "
    "Chinese_China.AL32UTF8</charset>"
    "<inifilename>/project/idc/ini/xmltodb.xml</inifilename>"
    "<xmlpath>/idcdata/xmltodb/vip</xmlpath><xmlpathbak>/idcdata/xmltodb/vipbak</xmlpathbak>"
    "<xmlpatherr>/idcdata/xmltodb/viperr</xmlpatherr>"
    "<timetvl>5</timetvl><timeout>50</timeout><pname>xmltodb_vip</pname>\"\n\n"
  );

  printf("本程序是共享平台的公共功能模块，用于把xml文件入库到Oracle的表中。\n");
  printf("logfilename   本程序运行的日志文件。\n");
  printf("xmlbuffer     本程序运行的参数，用xml表示，具体如下：\n\n");

  printf("connstr     数据库的连接参数，格式：username/passwd@tnsname。\n");
  printf(
    "charset     数据库的字符集，这个参数要与数据源数据库保持一致，否则会出现中文乱码的情况。\n"
  );
  printf("inifilename 数据入库的参数配置文件。\n");
  printf("xmlpath     待入库xml文件存放的目录。\n");
  printf("xmlpathbak  xml文件入库后的备份目录。\n");
  printf("xmlpatherr  入库失败的xml文件存放的目录。\n");
  printf(
    "timetvl     "
    "扫描xmlpath目录的时间间隔（执行入库任务的时间间隔），单位：秒，视业务需求而定，2-30之间。\n"
  );
  printf("timeout     本程序的超时时间，单位：秒，视xml文件大小而定，建议设置30以上。\n");
  printf("pname       进程名，尽可能采用易懂的、与其它进程不同的名称，方便故障排查。\n\n");
}

void app_exit(int sig) {
  logfile.write("程序退出，sig=%d\n\n", sig);
  conn.disconnect();
  exit(0);
}
