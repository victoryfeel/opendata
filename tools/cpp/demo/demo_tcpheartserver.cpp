#include "_public.h"
#include <csignal>

using namespace idc;

clogfile logfile;
ctcpserver tcpserver;
string str_sendbuf;
string str_recvbuf;

void parent_exit(const int sig);
void child_exit(const int sig);

bool biz_main(); // main task function
void biz001_login();
void biz002_querybalance();
void biz003_transfer();

int main(int argc, char* argv[]) {
  if (argc != 4) {
    printf("Usage:./demo_bankserver port logfile timeout\n");
    return -1;
  }
  // closeioandsignal(true);
  signal(SIGINT, parent_exit);
  signal(SIGTERM, parent_exit);
  // open logfile
  if (logfile.open(argv[2]) == false) {
    printf("logfile.open(%s) failed.\n", argv[2]);
    return -1;
  }


  //////////////////////////////////////////////////////////////
  // TODO: 1-init server, stablish listen fd
  //////////////////////////////////////////////////////////////

  if (tcpserver.initserver(atoi(argv[1])) == false) {
    logfile.write("tcpserver.initserver(%s) failed.\n", argv[1]);
    return -1;
  }


  //////////////////////////////////////////////////////////////
  // TODO: 2-main workflow
  //////////////////////////////////////////////////////////////

  while (true) {
    if (tcpserver.accept() == false) {
      logfile.write("tcpserver.accept() failed.\n");
      parent_exit(-1);
    }
    logfile.write("client (%s) connected.\n", tcpserver.getip());

    // 父进程回到accept,处理下一个client的连接请求
    if (fork() > 0) {
      tcpserver.closeclient();
      continue;
    }

    // set child exit signal
    signal(SIGINT, child_exit);
    signal(SIGTERM, child_exit);
    // child process only focus on task, so close listen fd
    tcpserver.closelisten();

    while (true) {
      if (tcpserver.read(str_recvbuf, atoi(argv[3])) == false) {
        logfile.write("tcpserver.read() failed.\n");
        child_exit(0);
      }
      logfile.write("recv: %s\n", str_recvbuf.c_str());

      // main task
      biz_main();

      if (tcpserver.write(str_sendbuf) == false) {
        logfile.write("tcpserver.write() failed.\n");
        child_exit(0);
      }
      logfile.write("send: %s\n", str_sendbuf.c_str());
    }

    child_exit(0);

    return 0;
  }
}


void biz003_transfer() {
  string fromcardid, tocardid;
  double money;
  getxmlbuffer(str_recvbuf, "fromcardid", fromcardid);
  getxmlbuffer(str_recvbuf, "tocardid", tocardid);
  getxmlbuffer(str_recvbuf, "money", money);

  // 假装操作了数据库，更新了两个账户的金额，完成了转帐操作。
  if (money < 100) {
    str_sendbuf = "<retcode>0</retcode><message>transfer success</message>";
    return;
  } else {
    str_sendbuf = "<retcode>-1</retcode><message>transfer failed,no enough money</message>";
  }
}

void biz002_querybalance() {
  string cardid;
  getxmlbuffer(str_recvbuf, "cardid", cardid);

  // 假装操作了数据库，得到了卡的余额。
  str_sendbuf = "<retcode>0</retcode><balance>128.83</balance>";
}

void biz001_login() {
  string username, password;
  getxmlbuffer(str_recvbuf, "username", username);
  getxmlbuffer(str_recvbuf, "password", password);

  if (username == "alex" && password == "225166") {
    str_sendbuf = "<retcode>0</retcode><message>login success</message>";
  } else {
    str_sendbuf = "<retcode>-1</retcode><message>invalid username or password</message>";
  }
}


bool biz_main() {
  int bizid;
  getxmlbuffer(str_recvbuf, "bizid", bizid);

  switch (bizid) {
  case 0:
    str_sendbuf = "<retcode>0</retcode><message>heart ok</message>";
    break;
  case 1:
    biz001_login();
    break;
  case 2:
    biz002_querybalance();
    break;
  case 3:
    biz003_transfer();
    break;
  default:
    str_sendbuf = "<retcode>-1</retcode><message>invalid bizid</message>";
  }

  return true;
}


void parent_exit(const int sig) {
  // 忽略广播信号, 防止parent_exit受影响
  signal(SIGINT, SIG_IGN);
  signal(SIGTERM, SIG_IGN);

  logfile.write("parent process, sig=%d.\n", sig);
  // pid=0,是指进程组的所有进程，包括父进程自己
  kill(0, 15);
  tcpserver.closelisten();

  exit(0);
}


void child_exit(const int sig) {
  // 忽略广播信号, 防止child_exit受影响
  signal(SIGINT, SIG_IGN);
  signal(SIGTERM, SIG_IGN);

  logfile.write("child process, sig=%d.\n", sig);
  tcpserver.closelisten();

  exit(0);
}