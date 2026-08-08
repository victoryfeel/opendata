#include "_ooci.h"
#include "_public.h"
using namespace idc;

void app_exit(int sig);
clogfile logfile;
cpactive pactive;
connection conn;
struct st_stcode {
  char provname[31]; // 省份
  char obtid[11];    // 站号
  char cityname[31]; // 站名
  char lat[11];      // 纬度
  char lon[11];      // 经度
  char height[11];   // 海拔高度
} stcode;
list<struct st_stcode> stcodelist;

bool loadstcode(const string& inifile);


int main(int argc, char* argv[]) {
  if (argc != 5) {
    printf("\n");
    printf("Using:./obtcodetodb inifile connstr charset logfile\n");

    printf(
      "Example:/project/tools/bin/procctl 120 /project/idc/bin/obtcodetodb "
      "/project/idc/ini/stcode.ini "
      "\"idc/idcpwd@snorcl11g_128\" \"Simplified Chinese_China.AL32UTF8\" "
      "/log/idc/obtcodetodb.log\n\n"
    );
    printf(
      "本程序用于把全国气象站点参数数据保存到数据库的T_"
      "ZHOBTCODE表中，如果站点不存在则插入，站点已存在则更新。\n"
    );
    printf("inifile 全国气象站点参数文件名（全路径）。\n");
    printf("connstr 数据库连接参数：username/password@tnsname\n");
    printf("charset 数据库的字符集。\n");
    printf("logfile 本程序运行的日志文件名。\n");
    printf("程序每120秒运行一次，由procctl调度。\n\n\n");
    return -1;
  }
  // closeioandsignal(true);
  signal(SIGINT, app_exit);
  signal(SIGTERM, app_exit);
  if (logfile.open(argv[4]) == false) {
    printf("logfile.open(%s) failed.\n", argv[4]);
    return -1;
  }

  pactive.addpinfo(10, "obtcodetodb");

  ///////////////////////////////////////////////////
  // TODO: 加载站点参数到容器, 编写loadstcode()实现
  ///////////////////////////////////////////////////
  if (loadstcode(argv[1]) == false) {
    app_exit(-1);
  }
  ///////////////////////////////////////////////////
  // TODO: 连接数据库，准备插入和更新表数据的sql语句
  ///////////////////////////////////////////////////
  if (conn.connecttodb(argv[2], argv[3]) != 0) {
    logfile.write("conn.connecttodb(%s,%s) failed.\n", argv[2], argv[3]);
    app_exit(-1);
  }
  logfile.write("connect database(%s) ok.\n", argv[2]);

  sqlstatement stmt_insert(&conn);
  stmt_insert.prepare(
    "insert into T_ZHOBTCODE(obtid,cityname,provname,lat,lon,height,keyid) "
    "values(:1,:2,:3,:4*100,:5*100,:6*10,SEQ_ZHOBTCODE.NEXTVAL)"
  );
  stmt_insert.bindin(1, stcode.obtid, 5);
  stmt_insert.bindin(2, stcode.cityname, 30);
  stmt_insert.bindin(3, stcode.provname, 30);
  stmt_insert.bindin(4, stcode.lat, 10);
  stmt_insert.bindin(5, stcode.lon, 10);
  stmt_insert.bindin(6, stcode.height, 10);

  sqlstatement stmt_update(&conn);
  stmt_update.prepare(
    "update T_ZHOBTCODE set "
    "cityname=:1,provname=:2,lat=:3*100,lon=:4*100,height=:5*10,upttime=sysdate where obtid=:6"
  );
  stmt_update.bindin(1, stcode.cityname, 30);
  stmt_update.bindin(2, stcode.provname, 30);
  stmt_update.bindin(3, stcode.lat, 10);
  stmt_update.bindin(4, stcode.lon, 10);
  stmt_update.bindin(5, stcode.height, 10);
  stmt_update.bindin(6, stcode.obtid, 5);
  // 记录insert, update的行数
  int insert_count = 0, update_count = 0;
  // 记录操作数据库消耗的时间
  ctimer timer;

  ///////////////////////////////////////////////////
  // TODO: 遍历容器，处理每个站点参数，插入表中，已存则更新数据
  ///////////////////////////////////////////////////
  for (auto& i : stcodelist) {
    stcode = i;
    if (stmt_insert.execute() != 0) {
      // 已经存在就update
      if (stmt_insert.rc() == 1) {
        if (stmt_update.execute() != 0) {
          logfile.write(
            "stmt_update.execute() failed.\n%s\n%s\n", stmt_update.sql(), stmt_update.message()
          );
          app_exit(-1);
        } else {
          update_count++;
        }
      } else {
        logfile.write(
          "stmt_insert.execute() failed.\n%s\n%s\n", stmt_insert.sql(), stmt_insert.message()
        );
      }
    } else {
      insert_count++;
    }
  }

  logfile.write(
    "总记录数=%d,insert=%d,update=%d,耗时=%.2f秒\n",
    stcodelist.size(),
    insert_count,
    update_count,
    timer.elapsed()
  );


  ///////////////////////////////////////////////////
  // TODO: 提交事务
  ///////////////////////////////////////////////////
  conn.commit();


  return 0;
}

bool loadstcode(const string& inifile) {
  cifile ifile;
  if (ifile.open(inifile) == false) {
    logfile.write("ifile.open(%s) failed.\n", inifile.c_str());
    return false;
  }

  string strbuffer;
  ccmdstr cmdstr;
  while (true) {
    if (ifile.readline(strbuffer) == false) {
      break;
    }
    // 拆分行
    cmdstr.splittocmd(strbuffer, ",");
    // 扔掉无效行
    if (cmdstr.cmdcount() != 6) {
      continue;
    }
    // 获取拆分后的字段内容，存入结构体变量中
    memset(&stcode, 0, sizeof(struct st_stcode));
    cmdstr.getvalue(0, stcode.provname, 30);
    cmdstr.getvalue(1, stcode.obtid, 5);
    cmdstr.getvalue(2, stcode.cityname, 30);
    cmdstr.getvalue(3, stcode.lat, 10);
    cmdstr.getvalue(4, stcode.lon, 10);
    cmdstr.getvalue(5, stcode.height, 10);

    stcodelist.push_back(stcode);
  }

  return true;
}

void app_exit(int sig) {
  printf("程序退出,sig=%d\n\n", sig);
  // 可不写因为在析构函数中会回滚事务和断开数据库连接
  conn.rollback();
  conn.disconnect();
  exit(0);
}
