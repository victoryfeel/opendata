#include "_public.h"
using namespace idc;

cpactive pactive;

void app_exit(const int sig);

int main(int argc, char* argv[]) {
  if (argc != 4) {
    printf("\n");
    printf("Using:/project/tools/bin/gzifiles pathname matchstr timeout\n\n");
    printf(
      "Example:/project/tools/bin/gzifiles /tmp/idc/surfdata "
      "\"*.xml,*.json\" 0.01\n"
    );
    cout << R"(Example2:/project/tools/bin/gzifiles/log/idc "*.log.20*" 0.01)" << endl;
    printf(
      "Example3:/project/tools/bin/procctl 300 "
      "project/tools/bin/gzipfiles /log/idc \"*.log20*\" 0.02\n\n"
    );
    printf(
      "Example4:/project/tools/bin/procctl 300 project/tools/bin/gzifiles "
      "/tmp/idc/surfdata \"*.xml,*json\" 0.01\n\n"
    );
    printf("这是一个工具程序，用来压缩历史数据文件或日志文件。\n");
    printf(
      "本程序会把pathname目录及其子目录中timeout天之前，且符合matchstr条件"
      "的文件进行压缩，timeout可以是小数。\n"
    );
    printf("本程序不会写日志，也不会在terminal上输出任何信息。\n");
    printf("本程序调用/usr/bin/gzip命令进行文件压缩。\n\n\n");

    return -1;
  }

  closeioandsignal(true);
  signal(SIGINT, app_exit);
  signal(SIGTERM, app_exit);

  pactive.addpinfo(120, "deletefiles");

  // TODO: 1-获取被定义为历史数据文件的时间点
  string strtimeout = ltime1("yyyymmddhh24miss", 0 - (int)(atof(argv[3]) * 24 * 60 * 60));
  // cout<<"strtimeout="<<strtimeout<<endl; //调试代码

  /*
   */
  // TODO: 2-打开目录
  cdir dir;
  if (dir.opendir(argv[1], argv[2], 10000, true, false) == false) {
    printf("dir.opendir(%s) failed.\n", argv[1]);
    return -1;
  }

  // TODO: 3-遍历目录中的文件，删除符合条件的历史数据文件
  // 把文件的时间与历史文件的时间点进行比较，如果更早，且不是压缩文件，则进行压缩。
  while (dir.readdir() == true) {
    if ((dir.m_mtime < strtimeout) && (matchstr(dir.m_filename, "*.gz") == false)) {
      string strcmd =
        "usr/bin/gzip -f " + dir.m_ffilename +
        "1>/dev/null 2>/dev/null"; // 构造压缩命令,/dev/null(1正确，2错误时）表示不输出任何信息

      // string command 由system函数调用执行
      if (system(strcmd.c_str()) == 0) {
        cout << "gzip " << dir.m_ffilename << " ok." << endl; // 调试代码
      } else {
        cout << "gzip " << dir.m_ffilename << " failed." << endl; // 调试代码
      }

      // 压缩文件的时间长，需要更新进程心跳；而删除文件的时间很短，不需要更新进程心跳
      pactive.uptatime();
    }
  }

  return 0;
}

void app_exit(const int sig) {
  printf("程序退出，sig=%d\n\n", sig);

  exit(0);
}
