/**
 * @file deletefiles.cpp
 * @author Alexandr Mak (alxndrmak@google.com)
 * @brief {这是一个工具程序，用来清理n天前的历史数据文件或日志文件。}
 * @version 0.1
 * @date 2026-04-03
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "_public.h"
using namespace idc;

// 添加进程的心跳,删除文件时间短，不需要更新心跳
cpactive pactive;

void app_exit(const int sig);

int main(int argc, char* argv[]) {
  if (argc != 4) {
    printf("\n");
    printf(
      "Using: this_program_name delete_files_directory_path "
      "file_matched_rule time_days_ago\n\n"
    );
    printf(
      "Example:/project/tools/bin/deletefiles /tmp/idc/surfdata "
      "\"*.xml,*.json\" 0.01\n"
    );
    cout << R"(Example2:/project/tools/bin/deletefiles /log/idc "*.log.20*" 0.01)" << endl;
    printf(
      "Example3:/project/tools/bin/procctl 300 "
      "project/tools/bin/deletefiles /log/idc \"*.log20*\" 0.02\n\n"
    );
    printf(
      "Example4:/project/tools/bin/procctl 300 project/tools/bin/deletefiles "
      "/tmp/idc/surfdata \"*.xml,*json\" 0.01\n\n"
    );
    printf("这是一个工具程序，用来清理历史数据文件或日志文件。\n");
    printf(
      "本程序会把pathname目录及其子目录中timeout天之前，且符合matchstr条件"
      "的文件删除掉，timeout可以是小数。\n"
    );
    printf("本程序不会写日志，也不会在terminal上输出任何信息。\n\n\n");

    return -1;
  }

  closeioandsignal(true); // 调试时可以注释掉这行代码
  signal(SIGINT, app_exit);
  signal(SIGTERM, app_exit);

  // 将进程的信息加入到共享内存中
  pactive.addpinfo(30, "deletefiles"); // 每30秒更新一次心跳

  // TODO:1-获取被定义为历史数据文件的时间点(多少天前的文件被定义为历史数据文件)
  // ltime1(fmt,offset), offset为0表当前时间，负数表过去，正数表未来
  string strtimeout = ltime1("yyyymmddhh24miss", 0 - (int)(atof(argv[3]) * 24 * 60 * 60));

  // TODO:2-打开目录
  cdir dir;
  if (dir.opendir(argv[1], argv[2], 10000, true, false) == false) {
    printf("dir.opendir(%s) failed.\n", argv[1]);
    return -1;
  }

  // TODO:3-遍历目录中的文件，删除符合条件的历史数据文件
  // 使用remove函数删除文件，成功返回0，失败返回-1
  while (dir.readdir() == true) {
    if (dir.m_mtime < strtimeout) {
      if (remove(dir.m_ffilename.c_str()) == 0) {
        cout << "delete file " << dir.m_ffilename << " ok." << endl; // 调试代码
      } else {
        cout << "delete file " << dir.m_ffilename << " failed." << endl; // 调试代码
      }
    }
  }

  return 0;
}

void app_exit(const int sig) {
  printf("程序退出，sig=%d\n\n", sig);
  exit(0);
}
