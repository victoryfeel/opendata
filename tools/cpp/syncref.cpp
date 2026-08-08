
#include "_tools.h"

clogfile logfile;
cpactive pactive;

struct st_arg {
  char localconnstr[101];  // 本地数据库的连接参数。
  char charset[51];        // 数据库的字符集。
  char linktname[31];      // dblink指向的远程表名，如T_ZHOBTCODE1@db128。
  char localtname[31];     // 本地表名。
  char remotecols[1001];   // 远程表的字段列表。
  char localcols[1001];    // 本地表的字段列表。
  char rwhere[1001];       // 同步数据的条件。
  char lwhere[1001];       // 同步数据的条件。
  int synctype;            // 同步方式：1-不分批刷新；2-分批刷新。
  char remoteconnstr[101]; // 远程数据库的连接参数。
  char remotetname[31];    // 远程表名。
  char remotekeycol[31];   // 远程表的键值字段名。
  char localkeycol[31];    // 本地表的键值字段名。
  int keylen;              // 键值字段的长度。
  int maxcount;            // 每批执行一次同步操作的记录数。
  int timeout;             // 本程序运行时的超时时间。
  char pname[51];          // 本程序运行时的程序名。
} starg;

connection connloc;
connection connrem;

void app_exit(int sig);
void app_help();
bool xml_to_arg(const char* strxmlbuffer);

void _syncref();

int main(int argc, char* argv[]) {
  if (argc != 3) {
    app_help();
    return -1;
  }
  // closeioandsignal(true);
  signal(SIGINT, app_exit);
  signal(SIGTERM, app_exit);
  if (logfile.open(argv[1]) == false) {
    printf("open logfile failed(%s)", argv[1]);
    return -1;
  }
  if (xml_to_arg(argv[2]) == false) {
    return -1;
  }

  pactive.addpinfo(starg.timeout, starg.pname);

  // connect to local db
  if (connloc.connecttodb(starg.localconnstr, starg.charset) != 0) {
    logfile.write("connect database(%s) failed.\n%s\n", starg.localconnstr, connloc.message());
    app_exit(-1);
  }
  // get columns for local db table if starg columns not provided
  if (strlen(starg.remotecols) == 0 || strlen(starg.localcols) == 0) {
    TableColumn tcols;
    if (tcols.get_column_info(connloc, starg.localtname) == false) {
      logfile.write("table(%s) doesn't exisit.\n", starg.localtname);
      app_exit(-1);
    }

    if (strlen(starg.remotecols) == 0) {
      strcpy(starg.remotecols, tcols.m_all_columns.c_str());
    }
    if (strlen(starg.localcols) == 0) {
      strcpy(starg.localcols, tcols.m_all_columns.c_str());
    }
  }

  // task function
  _syncref();


  return 0;
}


void _syncref() {
  ctimer timer;
}


bool xml_to_arg(const char* strxmlbuffer) {
  memset(&starg, 0, sizeof(struct st_arg));

  // 本地数据库的连接参数，格式：ip,username,password,dbname,port。
  getxmlbuffer(strxmlbuffer, "localconnstr", starg.localconnstr, 100);
  if (strlen(starg.localconnstr) == 0) {
    logfile.write("%s", "localconnstr is null.\n");
    return false;
  }

  // 数据库的字符集，这个参数要与远程数据库保持一致，否则会出现中文乱码的情况。
  getxmlbuffer(strxmlbuffer, "charset", starg.charset, 50);
  if (strlen(starg.charset) == 0) {
    logfile.write("%s", "charset is null.\n");
    return false;
  }

  // linktname表名。
  getxmlbuffer(strxmlbuffer, "linktname", starg.linktname, 30);
  if (strlen(starg.linktname) == 0) {
    logfile.write("%s", "linktname is null.\n");
    return false;
  }

  // 本地表名。
  getxmlbuffer(strxmlbuffer, "localtname", starg.localtname, 30);
  if (strlen(starg.localtname) == 0) {
    logfile.write("%s", "localtname is null.\n");
    return false;
  }

  // 远程表的字段列表，用于填充在select和from之间，所以，remotecols可以是真实的字段，也可以是函数
  // 的返回值或者运算结果。如果本参数为空，将用localtname表的字段列表填充。
  getxmlbuffer(strxmlbuffer, "remotecols", starg.remotecols, 1000);

  // 本地表的字段列表，与remotecols不同，它必须是真实存在的字段。如果本参数为空，将用localtname表的字段列表填充。
  getxmlbuffer(strxmlbuffer, "localcols", starg.localcols, 1000);

  // 同步数据的条件。
  getxmlbuffer(strxmlbuffer, "rwhere", starg.rwhere, 1000);
  getxmlbuffer(strxmlbuffer, "lwhere", starg.lwhere, 1000);

  // 同步方式：1-不分批刷新；2-分批刷新。
  getxmlbuffer(strxmlbuffer, "synctype", starg.synctype);
  if ((starg.synctype != 1) && (starg.synctype != 2)) {
    logfile.write("%s", "synctype is not in (1,2).\n");
    return false;
  }

  if (starg.synctype == 2) {
    // 远程数据库的连接参数，格式与localconnstr相同，当synctype==2时有效。
    getxmlbuffer(strxmlbuffer, "remoteconnstr", starg.remoteconnstr, 100);
    if (strlen(starg.remoteconnstr) == 0) {
      logfile.write("%s", "remoteconnstr is null.\n");
      return false;
    }

    // 远程表名，当synctype==2时有效。
    getxmlbuffer(strxmlbuffer, "remotetname", starg.remotetname, 30);
    if (strlen(starg.remotetname) == 0) {
      logfile.write("%s", "remotetname is null.\n");
      return false;
    }

    // 远程表的键值字段名，必须是唯一的，当synctype==2时有效。
    getxmlbuffer(strxmlbuffer, "remotekeycol", starg.remotekeycol, 30);
    if (strlen(starg.remotekeycol) == 0) {
      logfile.write("%s", "remotekeycol is null.\n");
      return false;
    }

    // 本地表的键值字段名，必须是唯一的，当synctype==2时有效。
    getxmlbuffer(strxmlbuffer, "localkeycol", starg.localkeycol, 30);
    if (strlen(starg.localkeycol) == 0) {
      logfile.write("%s", "localkeycol is null.\n");
      return false;
    }

    // 键值字段的大小。
    getxmlbuffer(strxmlbuffer, "keylen", starg.keylen);
    if (starg.keylen == 0) {
      logfile.write("%s", "keylen is null.\n");
      return false;
    }

    // 每批执行一次同步操作的记录数，当synctype==2时有效。
    getxmlbuffer(strxmlbuffer, "maxcount", starg.maxcount);
    if (starg.maxcount == 0) {
      logfile.write("%s", "maxcount is null.\n");
      return false;
    }
  }

  // 本程序的超时时间，单位：秒，视数据量的大小而定，建议设置30以上。
  getxmlbuffer(strxmlbuffer, "timeout", starg.timeout);
  if (starg.timeout == 0) {
    logfile.write("%s", "timeout is null.\n");
    return false;
  }

  // 本程序运行时的进程名，尽可能采用易懂的、与其它进程不同的名称，方便故障排查。
  getxmlbuffer(strxmlbuffer, "pname", starg.pname, 50);
  if (strlen(starg.pname) == 0) {
    logfile.write("%s", "pname is null.\n");
    return false;
  }

  return true;
}


void app_exit(int sig) {
  logfile.write("程序退出, sig=%d\n\n", sig);
  connloc.disconnect();
  exit(0);
}

void app_help() {
  printf("Using:/project/tools/bin/syncref logfilename xmlbuffer\n\n");

  printf("不分批同步，把T_ZHOBTCODE1@db128同步到T_ZHOBTCODE2。\n");
  printf(
    "Sample:/project/tools/bin/procctl 10 /project/tools/bin/syncref "
    "/log/idc/syncref_ZHOBTCODE2.log "
    "\"<localconnstr>idc/idcpwd@snorcl11g_128</localconnstr><charset>Simplified "
    "Chinese_China.AL32UTF8</charset>"
    "<linktname>T_ZHOBTCODE1@db128</linktname><localtname>T_ZHOBTCODE2</localtname>"
    "<remotecols>obtid,cityname,provname,lat,lon,height,upttime,keyid</remotecols>"
    "<localcols>stid,cityname,provname,lat,lon,height,upttime,recid</localcols>"
    "<rwhere>where obtid like '57%%%%'</rwhere><lwhere>where stid like '57%%%%'</lwhere>"
    "<synctype>1</synctype><timeout>50</timeout><pname>syncref_ZHOBTCODE2</pname>\"\n\n"
  );

  printf("分批同步，把T_ZHOBTCODE1@db128同步到T_ZHOBTCODE3。\n");
  printf(
    "因为测试的需要，xmltodb程序每次会删除T_ZHOBTCODE1@"
    "db128中的数据，全部的记录重新入库，keyid会变。\n"
  );
  printf("所以，以下脚本不能用keyid，要用obtid，用keyid会出问题，可以试试。\n");
  printf(
    "       /project/tools/bin/procctl 10 /project/tools/bin/syncref "
    "/log/idc/syncref_ZHOBTCODE3.log "
    "\"<localconnstr>idc/idcpwd@snorcl11g_128</localconnstr><charset>Simplified "
    "Chinese_China.AL32UTF8</charset>"
    "<linktname>T_ZHOBTCODE1@db128</linktname><localtname>T_ZHOBTCODE3</localtname>"
    "<remotecols>obtid,cityname,provname,lat,lon,height,upttime,keyid</remotecols>"
    "<localcols>stid,cityname,provname,lat,lon,height,upttime,recid</localcols>"
    "<rwhere>where obtid like '57%%%%'</rwhere>"
    "<synctype>2</synctype><remoteconnstr>idc/idcpwd@snorcl11g_128</remoteconnstr>"
    "<remotetname>T_ZHOBTCODE1</remotetname><remotekeycol>obtid</remotekeycol>"
    "<localkeycol>stid</localkeycol><keylen>5</keylen>"
    "<maxcount>10</maxcount><timeout>50</timeout><pname>syncref_ZHOBTCODE3</pname>\"\n\n"
  );

  printf("分批同步，把T_ZHOBTMIND1@db128同步到T_ZHOBTMIND2。\n");
  printf(
    "       /project/tools/bin/procctl 10 /project/tools/bin/syncref "
    "/log/idc/syncref_ZHOBTMIND2.log "
    "\"<localconnstr>idc/idcpwd@snorcl11g_128</localconnstr><charset>Simplified "
    "Chinese_China.AL32UTF8</charset>"
    "<linktname>T_ZHOBTMIND1@db128</linktname><localtname>T_ZHOBTMIND2</localtname>"
    "<remotecols>obtid,ddatetime,t,p,u,wd,wf,r,vis,upttime,keyid</remotecols>"
    "<localcols>stid,ddatetime,t,p,u,wd,wf,r,vis,upttime,recid</localcols>"
    "<rwhere>where ddatetime>sysdate-10/1440</rwhere>"
    "<synctype>2</synctype><remoteconnstr>idc/idcpwd@snorcl11g_128</remoteconnstr>"
    "<remotetname>T_ZHOBTMIND1</remotetname><remotekeycol>keyid</remotekeycol>"
    "<localkeycol>recid</localkeycol><keylen>15</keylen>"
    "<maxcount>10</maxcount><timeout>50</timeout><pname>syncref_ZHOBTMIND2</pname>\"\n\n"
  );

  printf("本程序是共享平台的公共功能模块，采用刷新的方法同步Oracle数据库之间的表。\n\n");

  printf("logfilename   本程序运行的日志文件。\n");
  printf("xmlbuffer     本程序运行的参数，用xml表示，具体如下：\n\n");

  printf("localconnstr  本地数据库的连接参数，格式：username/passwd@tnsname。\n");
  printf(
    "charset       "
    "数据库的字符集，这个参数要与本地和远程数据库保持一致，否则会出现中文乱码的情况。\n"
  );

  printf("linktname      dblink指向的远程表名，如T_ZHOBTCODE1@db128。\n");
  printf("localtname    本地表名，如T_ZHOBTCODE2。\n");

  printf(
    "remotecols    远程表的字段列表，用于填充在select和from之间，所以，remotecols可以是真实的字段，"
    "也可以是函数的返回值或者运算结果。如果本参数为空，就用localtname表的字段列表填充。\n"
  );
  printf(
    "localcols     本地表的字段列表，与remotecols不同，它必须是真实存在的字段。如果本参数为空，"
    "就用localtname表的字段列表填充。\n"
  );

  printf("rwhere        同步数据的条件，填充在远程表的查询语句之后，为空则表示同步全部的记录。\n");
  printf("lwhere        同步数据的条件，填充在本地表的删除语句之后，为空则表示同步全部的记录。\n");

  printf("synctype      同步方式：1-不分批刷新；2-分批刷新。\n");

  printf("remoteconnstr 远程数据库的连接参数，格式与localconnstr相同，当synctype==2时有效。\n");
  printf("remotetname   没有dblink的远程表名，当synctype==2时有效。\n");
  printf("remotekeycol  远程表的键值字段名，必须是唯一的，当synctype==2时有效。\n");
  printf("localkeycol   本地表的键值字段名，必须是唯一的，当synctype==2时有效。\n");
  printf("keylen        键值字段的长度，当synctype==2时有效。\n");
  printf("maxcount      执行一次同步操作的记录数，当synctype==2时有效。\n");

  printf("timeout       本程序的超时时间，单位：秒，视数据量的大小而定，建议设置30以上。\n");
  printf(
    "pname         本程序运行时的进程名，尽可能采用易懂的、与其它进程不同的名称，方便故障排查。\n\n"
  );
  printf(
    "注意：\n"
    "1）remotekeycol和localkeycol字段的选取很重要，如果是自增字段，那么在远程表中数据生成后自增字段"
    "的值不可改变，否则同步会失败；\n"
    "2）当远程表中存在delete操作时，无法分批刷新，因为远程表的记录被delete后就找不到了，无法从本地"
    "表中执行delete操作。\n\n\n"
  );
}
