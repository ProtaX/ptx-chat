#ifndef SHAREDUDEQUE_H_
#define SHAREDUDEQUE_H_

#include <stdint.h>

#include <mutex>
#include <condition_variable>
#include <deque>
#include <memory>
#include <utility>

namespace ptxchat {

template<typename T>
class SharedUDeque {
 public:
  SharedUDeque() noexcept: MAX_SIZE(1000) {}
  explicit SharedUDeque(size_t s) noexcept: MAX_SIZE(s) {}
  ~SharedUDeque() {}

  /**
   * \brief Get pointer to the front element
   * 
   * Blocks calling thread until the queue is empty.
   * Deletes empty pointer from the queue.
   */
  [[nodiscard]] std::unique_ptr<T> front() {
    std::unique_ptr<T> t;
    std::unique_lock<std::mutex> lc_q(mtx_);

    /* Wait until an item will be placed */
    cond_.wait(lc_q, [this] {
                return (!deque_.empty() || stop_);
              });
    if (stop_)
      return nullptr;

    t = std::move(deque_.front());
    deque_.pop_front();

    return t;
  }

  /**
   * \brief Get pointer to the back element
   * 
   * Blocks calling thread until the queue is empty.
   * Deletes empty pointer from the queue.
   */
  [[nodiscard]] std::unique_ptr<T> back() {
    std::unique_ptr<T> t;
    std::unique_lock<std::mutex> lc_q(mtx_);

    /* Wait until an item will be placed */
    cond_.wait(lc_q, [this] {
                return (!deque_.empty() || stop_);
              });
    if (stop_)
      return nullptr;

    t = std::move(deque_.back());
    deque_.pop_back();

    return t;
  }

  /**
   * \brief Push element to the front
   * 
   * Norifies one waiting thread (in front() or back())
   */
  bool push_front(std::unique_ptr<T>&& i) {
    std::unique_lock<std::mutex> lc_q(mtx_);
    if (deque_.size() == MAX_SIZE)
      return false;

    deque_.push_front(std::move(i));
    mtx_.unlock();
    cond_.notify_one();
    return true;
  }

  /**
   * \brief Push element to the back
   * 
   * Norifies one waiting thread (in front() or back())
   */
  bool push_back(std::unique_ptr<T>&& i) {
    std::unique_lock<std::mutex> lc_q(mtx_);
    if (deque_.size() == MAX_SIZE)
      return false;

    deque_.push_back(std::move(i));
    mtx_.unlock();
    cond_.notify_one();
    return true;
  }

  void stop() {
    stop_ = true;
    cond_.notify_all();
  }

 private:
  mutable std::mutex mtx_;
  std::condition_variable cond_;
  std::deque<std::unique_ptr<T>> deque_;

  volatile bool stop_ = false;
  const size_t MAX_SIZE;
};

}  // namespace ptxchat

#endif  // SHAREDUDEQUE_H_
