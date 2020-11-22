#ifndef THREADS_H_
#define THREADS_H_

#include <thread>
#include <condition_variable>
#include <mutex>

struct ThreadState {
  std::thread thread;
  volatile int stop;
};

static volatile int ZERO = 0;

inline void PtxChatCrash() {
  ZERO = 1 / ZERO;
}

#endif  // THREADS_H_
