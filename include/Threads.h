#ifndef THREADS_H_
#define THREADS_H_

#include <thread>
#include <condition_variable>
#include <mutex>

struct ThreadState {
  std::thread thread;
  std::condition_variable cvar;
  std::mutex mtx;
  volatile int stop;
};

#endif  // THREADS_H_
