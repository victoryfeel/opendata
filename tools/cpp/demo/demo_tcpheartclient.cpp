#include "_public.h"
using namespace idc;

ctcpclient tcpclient;
string str_sendbuf;
string str_recvbuf;

bool biz000_heart(const int timeout);
bool biz001_login(const int timeout);
bool biz002_querybalance(const int timeout);
bool biz003_transfer(const int timeout);

int main(int argc, char* argv[]) {
  if (argc != 4) {
    printf("Usage:./demo_bankclient ip port timeout\n");
    return -1;
  }

  if (tcpclient.connect(argv[1], atoi(argv[2])) == false) {
    printf("tcpclient.connect failed.\n");
    return -1;
  }

  biz001_login(atoi(argv[3]));
  biz002_querybalance(atoi(argv[3]));

  sleep(6);
  biz000_heart(atoi(argv[3]));
  sleep(6);

  biz003_transfer(atoi(argv[3]));

  return 0;
}


bool biz000_heart(const int timeout) {
  str_sendbuf = "<bizid>0</bizid>";
  if (tcpclient.write(str_sendbuf.c_str()) == false) {
    printf("tcpclient.write failed.\n");
    return false;
  }
  cout << "send: " << str_sendbuf << endl;

  if (tcpclient.read(str_recvbuf, timeout) == false) {
    printf("tcpclient.read failed.\n");
    return false;
  }
  cout << "recv: " << str_recvbuf << endl;

  return true;
}


bool biz003_transfer(const int timeout) {
  str_sendbuf =
    "<bizid>3</bizid><fromcardid>alex</fromcardid><tocardid>bob</tocardid><money>811.66</money>";
  if (tcpclient.write(str_sendbuf.c_str()) == false) {
    printf("tcpclient.write failed.\n");
    return false;
  }
  cout << "send: " << str_sendbuf << endl;

  if (tcpclient.read(str_recvbuf, timeout) == false) {
    printf("tcpclient.read failed.\n");
    return false;
  }
  cout << "recv: " << str_recvbuf << endl;

  return true;
}


bool biz002_querybalance(const int timeout) {
  str_sendbuf = "<bizid>2</bizid><cardid>alex</cardid>";
  if (tcpclient.write(str_sendbuf.c_str()) == false) {
    printf("tcpclient.write failed.\n");
    return false;
  }
  cout << "send: " << str_sendbuf << endl;

  if (tcpclient.read(str_recvbuf, timeout) == false) {
    printf("tcpclient.read failed.\n");
    return false;
  }
  cout << "recv: " << str_recvbuf << endl;

  return true;
}


bool biz001_login(const int timeout) {
  str_sendbuf = "<bizid>1</bizid><username>alex</username><password>225166</password>";
  if (tcpclient.write(str_sendbuf.c_str()) == false) {
    printf("tcpclient.write failed.\n");
    return false;
  }
  cout << "send: " << str_sendbuf << endl;

  if (tcpclient.read(str_recvbuf, timeout) == false) {
    printf("tcpclient.read failed.\n");
    return false;
  }
  cout << "recv: " << str_recvbuf << endl;

  return true;
}