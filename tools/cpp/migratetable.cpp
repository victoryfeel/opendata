#include "_public.h"
#include "_tools.h"
using namespace idc;

clogfile logfile;
connection conn;
cpactive pactive;
void app_exit(int sig);
void app_help();
bool xmltoargs(const char* xmlbuffer);
bool instarttime();

bool _migratetable();

struct st_arg {
  char constr[101];
  char tname[31];     // 待迁移的表名
  char totname[31];   // 迁移target表的表名
  char keycol[31];    // 待清理表名的唯一键字段,使用rowid
  char where[1001];   // 满足清理的条件
  int maxcount;       // 执行一次sql删除的记录数
  char starttime[31]; // 程序运行时间
  int timeout;
  char pname[51];
} starg;

int main(int argc, char* argv[]) {
  if (argc != 3) {
    app_help();
    return -1;
  }
  closeioandsignal(true);
  signal(SIGINT, app_exit);
  signal(SIGTERM, app_exit);
  if (logfile.open(argv[1]) == false) {
    printf("logfile.open(%s) failed.\n", argv[1]);
    return -1;
  }
  if (xmltoargs(argv[2]) == false) {
    logfile.write("xmltoargs(%s) failed.\n", argv[2]);
    return -1;
  }
  if (instarttime() == false) {
    return 0;
  }
  pactive.addpinfo(starg.timeout, starg.pname);

  // connect to database, 不启用自动提交事务
  // 因为操作的是rowid,不存在中文，所以字符集随便填
  if (conn.connecttodb(starg.constr, "Simplified Chinese_China.AL32UTF8") != 0) {
    logfile.write("connect database(%s) failed.\n%s\n", starg.constr, conn.message());
    app_exit(-1);
  }

  _migratetable();

  return 0;
}


bool _migratetable() {
  ctimer timer;
  char singlecol_unique_value[21];
  sqlstatement stmt_select(&conn);
  sqlstatement stmt_delete(&conn);
  //////////////////////////////////////////////////////////////////////////////////////////
  ////// 1. 准备select, delete sql语句
  //////////////////////////////////////////////////////////////////////////////////////////

  // select sql: select rowid from T_ZHOBTMIND1 where ddatetime<sysdate-1
  stmt_select.prepare("select %s from %s %s", starg.keycol, starg.tname, starg.where);
  stmt_select.bindout(1, singlecol_unique_value, 20);
  // delete sql: delete from T_ZHOBTMIND1 where rowid in (:1,:2,:3,:4,:5,:6,:7,:8,:9,:10);
  string delete_sql = sformat("delete %s where %s in (", starg.tname, starg.keycol);
  for (int i = 0; i < starg.maxcount; i++) {
    delete_sql = delete_sql + sformat(":%lu,", i + 1);
  }
  deleterchr(delete_sql, ',');
  delete_sql = delete_sql + ")";

  stmt_delete.prepare(delete_sql);
  char unique_values[starg.maxcount][21];
  for (int i = 0; i < starg.maxcount; i++) {
    stmt_delete.bindin(i + 1, unique_values[i], 20);
  }
  //////////////////////////////////////////////////////////////////////////////////////////
  //////  99. 准备insert sql语句
  //////////////////////////////////////////////////////////////////////////////////////////

  // insert into T_ZHOBTMIND1_HIS(全部的字段) select 全部的字段 from T_ZHOBTMIND1
  // where rwoid in (:1,:2,:3,:4,:5,:6,:7,:8,:9,:10);
  // 不能用*代替全部字段，因为两个表的字段顺序可能不一样，程序中的sql语句不应该出现*
  TableColumn tcol;
  tcol.get_column_info(conn, starg.tname);
  string insert_sql = sformat(
    "insert into %s select %s from %s where %s in (",
    starg.totname,
    tcol.m_all_columns.c_str(),
    starg.tname,
    starg.keycol
  );
  for (int i = 0; i < starg.maxcount; i++) {
    insert_sql = insert_sql + sformat(":%lu,", i + 1);
  }
  deleterchr(insert_sql, ',');
  insert_sql = insert_sql + ")";
  sqlstatement stmt_insert(&conn);
  stmt_insert.prepare(insert_sql);
  for (int i = 0; i < starg.maxcount; i++) {
    stmt_insert.bindin(i + 1, unique_values[i], 20);
  }

  //////////////////////////////////////////////////////////////////////////////////////////
  ////// 2. 获取记录的唯一字段值rowid, 将每一行的rowid转存到二维数组存储
  //////////////////////////////////////////////////////////////////////////////////////////
  if (stmt_select.execute() != 0) {
    logfile.write(
      "stmt_select.execute() failed.\n%s\n%s\n", stmt_select.sql(), stmt_select.message()
    );
    return false;
  }

  //////////////////////////////////////////////////////////////////////////////////////////
  ////// 3. rowid二维数组满了之后，执行delete sql进行批量删除
  //////////////////////////////////////////////////////////////////////////////////////////
  int valid_unique_values_count = 0;
  memset(unique_values, 0, sizeof(unique_values));
  while (true) {
    // rowid转存到unique_values数组
    memset(singlecol_unique_value, 0, sizeof(singlecol_unique_value));
    if (stmt_select.next() != 0) {
      break;
    }
    strcpy(unique_values[valid_unique_values_count], singlecol_unique_value);
    valid_unique_values_count++;

    // 数量到达maxcount,则实行一次insert_sql, delete_sql
    if (valid_unique_values_count == starg.maxcount) {
      if (stmt_insert.execute() != 0) {
        logfile.write(
          "stmt_insert.execute() failed.\n%s\n%s\n", stmt_insert.sql(), stmt_insert.message()
        );
      }
      if (stmt_delete.execute() != 0) {
        logfile.write(
          "stmt_delete.execute() failed.\n%s\n%s\n", stmt_delete.sql(), stmt_delete.message()
        );
        return false;
      }
      conn.commit();
      valid_unique_values_count = 0;
      memset(unique_values, 0, sizeof(unique_values));
      pactive.uptatime();
    }
  }

  //////////////////////////////////////////////////////////////////////////////////////////
  ////// 4. rowid二维数组不够maxcount的剩余记录
  //////////////////////////////////////////////////////////////////////////////////////////
  if (valid_unique_values_count > 0) {
    if (stmt_insert.execute() != 0) {
      logfile.write(
        "stmt_insert.execute() failed.\n%s\n%s\n", stmt_insert.sql(), stmt_insert.message()
      );
    }
    if (stmt_delete.execute() != 0) {
      logfile.write(
        "stmt_delete.execute() failed.\n%s\n%s\n", stmt_delete.sql(), stmt_delete.message()
      );
      return false;
    }
    conn.commit();
  }

  // total delete log
  if (stmt_select.rpc() > 0) {
    logfile.write(
      "migrate %s to %s %d rows in %.02f seconds.\n",
      starg.tname,
      starg.totname,
      stmt_select.rpc(),
      timer.elapsed()
    );
  }

  return true;
}


bool instarttime() {
  if (strlen(starg.starttime) != 0) {
    if (strstr(starg.starttime, ltime1("24hh").c_str()) == nullptr) {
      return false;
    }
  }
  return true;
}

bool xmltoargs(const char* xmlbuffer) {
  memset(&starg, 0, sizeof(struct st_arg));
  getxmlbuffer(xmlbuffer, "connstr", starg.constr, 100);
  if (strlen(starg.constr) == 0) {
    logfile.write("%s", "connstr is null.\n");
    return false;
  }
  // getxmlbuffer错误用法, <constr></constr>这种有标签但没有值，仍然会返回true
  // if (getxmlbuffer(xmlbuffer, "connstr", starg.constr, 100) == false) {
  //   logfile.write("%s", "connstr is null.\n");
  //   return false;
  // }
  getxmlbuffer(xmlbuffer, "tname", starg.tname, 30);
  if (strlen(starg.tname) == 0) {
    logfile.write("%s", "tname is null.\n");
    return false;
  }
  getxmlbuffer(xmlbuffer, "totname", starg.totname, 30);
  if (strlen(starg.totname) == 0) {
    logfile.write("%s", "totname is null.\n");
    return false;
  }
  getxmlbuffer(xmlbuffer, "keycol", starg.keycol, 30);
  if (strlen(starg.keycol) == 0) {
    logfile.write("%s", "keycol is null.\n");
    return false;
  }
  getxmlbuffer(xmlbuffer, "where", starg.where, 1000);
  if (strlen(starg.where) == 0) {
    logfile.write("%s", "where is null.\n");
    return false;
  }
  getxmlbuffer(xmlbuffer, "maxcount", starg.maxcount);
  if (starg.maxcount == 0) {
    logfile.write("%s", "maxcount is null.\n");
    return false;
  }

  getxmlbuffer(xmlbuffer, "starttime", starg.starttime, 30);

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

void app_help() {
  printf("Using:/project/tools/bin/migratetable logfilename xmlbuffer\n\n");

  printf(
    "Sample:/project/tools/bin/procctl 3600 /project/tools/bin/migratetable "
    "/log/idc/migratetable_ZHOBTMIND1.log "
    "\"<connstr>idc/idcpwd@snorcl11g_128</connstr><tname>T_ZHOBTMIND1</tname>"
    "<totname>T_ZHOBTMIND1_HIS</totname><keycol>rowid</keycol><where>where "
    "ddatetime<sysdate-0.03</where>"
    "<maxcount>10</maxcount><starttime>22,23,00,01,02,03,04,05,06,13</starttime>"
    "<timeout>120</timeout><pname>migratetable_ZHOBTMIND1</pname>\"\n\n"
  );

  printf("本程序是共享平台的公共功能模块，用于迁移表中的数据。\n");
  printf("logfilename 本程序运行的日志文件。\n");
  printf("xmlbuffer   本程序运行的参数，用xml表示，具体如下：\n\n");
  printf("connstr     数据库的连接参数，格式：username/passwd@tnsname。\n");
  printf("tname       待迁移数据表的表名，例如T_ZHOBTMIND1。\n");
  printf("totname     目的表名，例如T_ZHOBTMIND1_HIS。\n");
  printf(
    "keycol      待迁移数据表的唯一键字段名，可以用记录编号，如keyid，建议用rowid，效率最高。\n"
  );
  printf("where       待迁移的数据需要满足的条件，即SQL语句中的where部分。\n");
  printf("maxcount    执行一次SQL语句删除的记录数，建议在100-500之间。\n");
  printf(
    "starttime   "
    "程序运行的时间区间，例如02,13表示：如果程序运行时，踏中02时和13时则运行，其它时间不运行。"
    "如果starttime为空，本参数将失效，只要本程序启动就会执行数据迁移，"
    "为了减少对数据库的压力，数据迁移一般在业务最闲的时候时进行。\n"
  );
  printf("timeout     本程序的超时时间，单位：秒，建议设置120以上。\n");
  printf("pname       进程名，尽可能采用易懂的、与其它进程不同的名称，方便故障排查。\n\n");
}

void app_exit(int sig) {
  logfile.write("程序退出，sig=%d\n\n", sig);
  conn.disconnect();
  exit(0);
}
