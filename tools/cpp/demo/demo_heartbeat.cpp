/**
 * @file server_test.cpp
 * @author Alex Mak (aliasgmail@duck.com)
 * @brief {进程的心跳}
 * @version 0.1
 * @date 2026-05-09
 *
 * @copyright Copyright (c) 2026
 *
 */


#include <iostream>

#include <_public.h>
#include <cstdlib>
using namespace idc;

struct st_ProgramInfo {
  int pid;
  char program_name[51] = {0};
  int time_out;
  time_t last_time;
};
int shared_memory_id = -1;
st_ProgramInfo* shared_memory_ptr = nullptr;
int memory_index = -1; // 当前进程在共享内存数组中的索引下标

void app_exit(const int sig) {
  std::cout << "程序退出，sig=" << sig << std::endl;
  // TODO: 7-从共享内存中删除当前进程信息
  if (memory_index != -1) {
    memset(&shared_memory_ptr[memory_index], 0, sizeof(st_ProgramInfo));
  }
  // TODO： 8-断开当前进程与共享内存的连接
  if (shared_memory_ptr != nullptr) {
    shmdt(shared_memory_ptr);
  }
  exit(0);
}

int main() {
  // TODO: 1-处理程序退出信号
  signal(SIGINT, app_exit);
  signal(SIGTERM, app_exit);

  // TODO：2-创建共享内存
  shared_memory_id = shmget(0x5095, 1000 * sizeof(st_ProgramInfo), 0666 | IPC_CREAT);
  if (shared_memory_id == -1) {
    printf("get shared memory failed.\n");
    return -1;
  }
  // TODO：3-将共享内存连接到当前进程的地址空间
  shared_memory_ptr = reinterpret_cast<st_ProgramInfo*>(shmat(shared_memory_id, nullptr, 0));
  // TODO：4-将当前进程信息写入结构体
  st_ProgramInfo program_info;
  memset(&program_info, 0, sizeof(program_info));
  program_info.pid = getpid();
  strncpy(program_info.program_name, "server", 50);
  program_info.time_out = std::chrono::seconds(30).count();
  program_info.last_time = time(nullptr); // 当前时间

  // TODO:优化2-使用信号量加锁，防止多个进程同时访问共享内存导致数据混乱
  // 涉及共享内存的操作有：找空内存+写入共享内存、更新进程心跳时间、删除共享内存中的当前进程信息(各自操作自己的结构体不用加锁)
  // 加锁位置：在找空内存之前加锁,写入共享内存时加锁(多个进程可能会找到同一个位置)
  // 解锁位置：找到空内存后解锁，写入共享内存后解锁
  csemp semlock;
  // 初始化信号量
  if (semlock.init(0x5095) == false) {
    printf("create semaphore failed.\n");
    app_exit(-1);
  }

  // 找之前加锁
  semlock.wait();

  // TODO：5-找一块空内存，将结构体放入共享内存

  // TODO:优化1：进程id会重用，所以先找一块内存，看看之前的进程id是否和当前进程id相同
  // 如果相同则说明是同一个程序重启了，可以复用这块内存，否则就找一块空内存。

  // 找到旧位置
  for (int i = 0; i < 1000; i++) {
    if (shared_memory_ptr[i].pid == program_info.pid) { // 找到旧内存
      memory_index = i;
      std::cout << "找到旧内存，索引下标=" << memory_index << std::endl;
      break;
    }
  }
  // 找新位置
  for (int i = 0; i < 1000; i++) {
    if (shared_memory_ptr[i].pid == 0) { // 找到空内存
      memory_index = i;
      std::cout << "找到新的空内存，索引下标=" << memory_index << std::endl;
      break;
    }
  }

  if (memory_index == -1) {
    semlock.post(); // 解锁
    printf("没有空内存了.\n");
    app_exit(-1);
  }
  // 将当前进程信息写入共享内存
  memcpy(&shared_memory_ptr[memory_index], &program_info, sizeof(program_info));

  semlock.post(); // 解锁

  while (true) {
    printf("程序正在运行中...\n");
    // TODO: 6-更新进程的心跳信息
    // 更新时间间隔一定要小于time_out的值, 如果大于则分两次更新,如sleep(50)
    sleep(25);
    shared_memory_ptr[memory_index].last_time = time(nullptr);
    sleep(25);
    shared_memory_ptr[memory_index].last_time = time(nullptr);
  }

  return 0;
}