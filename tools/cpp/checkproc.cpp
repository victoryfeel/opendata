#include "_public.h"
using namespace idc;

int main(int argc, char* argv[]) {
  if (argc != 2) {
    printf("Using: this_program_name logfile_name\n");
    printf(
      "Example:/project/tools/bin/procctl 10 /project/tools/bin/checkproc "
      "/tmp/log/checkproc.log\n"
    );
    printf("本程序用于检查后台服务程序是否超时，如果已超时，就终止它。\n");
    printf("注意：\n");
    printf("  1）本程序由procctl启动，运行周期建议为10秒。\n");
    printf("  2）为了避免被普通用户误杀，本程序应该用root用户启动。\n");
    printf("  3）如果要停止本程序，只能用killall -9 终止。\n\n\n");
    return -1;
  }

  // 守护模块不处理任何的信号IO
  closeioandsignal(true);

  // TODO: 1-打开日志文件
  clogfile log_file;
  if (log_file.open(argv[1]) == false) {
    printf("打开日志文件失败（%s）。\n", argv[1]);
    return -1;
  }

  // TODO:
  // 2-获取共享内存,key为SHMKEYP,大小为MAXNUMP个st_procinfo结构体的大小，权限为0666
  int shmid = -1;
  if (
    (shmid = shmget(
       static_cast<key_t>(SHMKEYP), MAXNUMP * sizeof(struct st_procinfo), 0666 | IPC_CREAT
     )) == -1
  ) {
    log_file.write("创建共享内存失败（%s）。\n", SHMKEYP);
    return -1;
  }

  // TODO: 3-连接共享内存到当前的进程地址空间
  st_procinfo* shm = reinterpret_cast<st_procinfo*>(shmat(shmid, nullptr, 0));

  // TODO: 4-遍历共享内存中的所有服务程序，检查它们是否超时，如果超时就终止它们
  for (int i = 0; i < MAXNUMP; i++) {
    // pid==0, 没有程序信息记录在这个位置
    if (shm[i].pid == 0) {
      continue;
    }

    // 调试:显示进程信息
    // log_file.write("ii=%d,pid=%d,pname=%s,timeout=%d,atime=%d\n", i,
    // shm[i].pid,
    //                shm[i].pname, shm[i].timeout, shm[i].atime);

    // 判断进程是否存在
    int iret = kill(shm[i].pid, 0); // 0号信号不发送信号，只判断进程是否存在
    if (iret == -1) {
      log_file.write("进程（%d）%s已不存在\n", shm[i].pid, shm[i].pname);
      memset(&shm[i], 0, sizeof(struct st_procinfo)); // 将进程残留信息清除
      continue;
    }

    // 判断是否超时
    time_t time_now = time(nullptr);
    if ((time_now - shm[i].atime) < shm[i].timeout) {
      continue;
    }

    // TODO: 4.1-将进程信息结构体的copy一份
    st_procinfo tmp_procinfo = shm[i];
    if (tmp_procinfo.pid == 0) {
      continue;
    }

    // 超时了，先显示日志
    log_file.write("进程（%d）%s已超时\n", tmp_procinfo.pid, tmp_procinfo.pname);

    /*
      TODO: 4.2-然后终止它
      1.先使用15信号终止;
      2.再每隔1s判断进程是否存在,累计5s；（5s足够时间让进程收尾退出）
      3.如果5s后进程还存在，就使用9信号强行终止。
    */

    kill(tmp_procinfo.pid, 15);
    for (int i = 1; i <= 5; i++) {
      sleep(1);
      iret = kill(tmp_procinfo.pid, 0);
      if (iret == -1) {
        break;
      }
    }
    if (iret == -1) {
      log_file.write("进程（%d）%s已正常终止\n", tmp_procinfo.pid, tmp_procinfo.pname);
    } else {
      kill(tmp_procinfo.pid, 9);
      log_file.write("进程（%d）%s被强行终止\n", tmp_procinfo.pid, tmp_procinfo.pname);
      memset(&shm[i], 0, sizeof(struct st_procinfo)); // 将进程残留信息清除
    }
  }

  // TODO: 5-断开共享内存
  shmdt(shm);

  return 0;
}
