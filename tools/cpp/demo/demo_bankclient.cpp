#include "_public.h"
using namespace idc;

ctcpclient tcpclient;
string str_sendbuf;
string str_recvbuf;

bool biz001_login();
bool biz002_querybalance();
bool biz003_transfer();

int main(int argc, char* argv[]) {
  if (argc != 3) {
    printf("Usage:./demo_bankclient ip port\n");
    return -1;
  }

  if (tcpclient.connect(argv[1], atoi(argv[2])) == false) {
    printf("tcpclient.connect failed.\n");
    return -1;
  }

  biz001_login();
  biz002_querybalance();
  biz003_transfer();

  return 0;
}


bool biz003_transfer() {
  str_sendbuf =
    "<bizid>3</bizid><fromcardid>alex</fromcardid><tocardid>bob</tocardid><money>811.66</money>";
  if (tcpclient.write(str_sendbuf.c_str()) == false) {
    printf("tcpclient.write failed.\n");
    return false;
  }
  cout << "send: " << str_sendbuf << endl;

  if (tcpclient.read(str_recvbuf) == false) {
    printf("tcpclient.read failed.\n");
    return false;
  }
  cout << "recv: " << str_recvbuf << endl;

  return true;
}


bool biz002_querybalance() {
  str_sendbuf = "<bizid>2</bizid><cardid>alex</cardid>";
  if (tcpclient.write(str_sendbuf.c_str()) == false) {
    printf("tcpclient.write failed.\n");
    return false;
  }
  cout << "send: " << str_sendbuf << endl;

  if (tcpclient.read(str_recvbuf) == false) {
    printf("tcpclient.read failed.\n");
    return false;
  }
  cout << "recv: " << str_recvbuf << endl;

  return true;
}


bool biz001_login() {
  str_sendbuf = "<bizid>1</bizid><username>alex</username><password>225166</password>";
  if (tcpclient.write(str_sendbuf.c_str()) == false) {
    printf("tcpclient.write failed.\n");
    return false;
  }
  cout << "send: " << str_sendbuf << endl;

  if (tcpclient.read(str_recvbuf) == false) {
    printf("tcpclient.read failed.\n");
    return false;
  }
  cout << "recv: " << str_recvbuf << endl;

  return true;
}