#include "idcapp.h"
using namespace idc;
/* Q&A
 * 1. 只插入不更新，因为是由气象站自动生成收集。而气象站参数可修改是因为气象站可能会迁移。
 * 2. 判断数据库是否连接：数据库比较忙，能不连就不连。
 */

void app_exit(int sig);
clogfile logfile;
cpactive pactive;
connection conn;
bool _obtmindtodb(const char* pathname, const char* connstr, const char* charset);


int main(int argc, char* argv[]) {
  if (argc != 5) {
    printf("\n");
    printf("Using:./obtmindtodb pathname connstr charset logfile\n");
    printf(
      "Example:/project/tools/bin/procctl 10 /project/idc/bin/obtmindtodb /idcdata/surfdata "
      "\"idc/idcpwd@snorcl11g_128\" \"Simplified Chinese_China.AL32UTF8\" "
      "/log/idc/obtmindtodb.log\n\n"
    );
    printf(
      "本程序用于把全国气象观测数据文件入库到T_"
      "ZHOBTMIND表中，支持xml和csv两种文件格式，数据只插入，不更新。\n"
    );
    printf("pathname 全国气象观测数据文件存放的目录。\n");
    printf("connstr  数据库连接参数：username/password@tnsname\n");
    printf("charset  数据库的字符集。\n");
    printf("logfile  本程序运行的日志文件名。\n");
    printf("程序每10秒运行一次，由procctl调度。\n\n\n");
    return -1;
  }
  // closeioandsignal(true);
  signal(SIGINT, app_exit);
  signal(SIGTERM, app_exit);
  if (logfile.open(argv[4]) == false) {
    printf("logfile.open(%s) failed.\n", argv[4]);
    return -1;
  }
  pactive.addpinfo(30, "obtmindtodb");

  // main function
  _obtmindtodb(argv[1], argv[2], argv[3]);

  return 0;
}


bool _obtmindtodb(const char* pathname, const char* connstr, const char* charset) {
  // 打开pathname, 循环读取目录下的xml,csv文件
  cdir dir;
  if (dir.opendir(pathname, "*.xml,*.csv") == false) {
    logfile.write("dir.opendir(%s) failed.\n", pathname);
    return false;
  }
  CZHOBTMIND ZHOBTMIND(conn, logfile);

  while (true) {
    // 读取一个气象观测数据文件（只处理*.xml和*.csv）
    if (dir.readdir() == false) {
      break;
    }
    // 如有文件需要处理，先判断是否连上数据库，再准备sql语句
    if (conn.isopen() == false) {
      if (conn.connecttodb(connstr, charset) != 0) {
        logfile.write("conncet database(%s) failed. \n%s\n", connstr, conn.message());
        return false;
      }
      logfile.write("connect database(%s) ok.\n", connstr);
    }
    // 打开文件读取
    cifile ifile;
    if (ifile.open(dir.m_ffilename) == false) {
      logfile.write("ifile.open(%s) failed.\n", dir.m_ffilename.c_str());
      return false;
    }

    int totalcount = 0, insertcount = 0;
    ctimer timer;
    string strbuffer;
    bool bisxml = matchstr(dir.m_ffilename, "*.xml");
    if (bisxml == false) {
      ifile.readline(strbuffer);
    }

    while (true) {
      if (bisxml == true) {
        if (ifile.readline(strbuffer, "<endl/>") == false) {
          break;
        }
      } else {
        if (ifile.readline(strbuffer) == false) {
          break;
        }
      }
      totalcount++;
      ZHOBTMIND.splitbuffer(strbuffer, bisxml);
      if (ZHOBTMIND.inserttable() == true) {
        insertcount++;
      }
    }
    // closeandremove已经处理的文件，提交事务
    ifile.closeandremove();
    conn.commit();
    logfile.write(
      "已处理%s (totalcount=%d,insertcount=%d,耗时=%.2f秒)\n",
      dir.m_ffilename.c_str(),
      totalcount,
      insertcount,
      timer.elapsed()
    );
    pactive.uptatime();
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
