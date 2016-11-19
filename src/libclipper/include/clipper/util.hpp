#ifndef CLIPPER_LIB_UTIL_H
#define CLIPPER_LIB_UTIL_H

#include <condition_variable>
#include <mutex>
#include <shared_mutex>
#include <queue>
// uncomment to disable assert()
// #define NDEBUG
#include <cassert>

#include "boost/optional.hpp"

#define UNUSED(expr) \
  do {               \
    (void)(expr);    \
  } while (0)

namespace clipper {

// Queue implementation borrowed from LatticeFlow
// https://github.com/ucbrise/LatticeFlow/blob/3d9e2fa9d84d8a5f578c0039f9ee6f3307cf8b1b/src/concurrency/queue.h
template <typename T>
class Queue {
 public:
  Queue() = default;
  explicit Queue(std::vector<T> xs) : xs_(std::move(xs)) {}
  Queue(const Queue&) = delete;
  Queue& operator=(const Queue&) = delete;

  // TODO should we allow move constructors?
  Queue(Queue&&) = delete;
  Queue& operator=(Queue&&) = delete;

  void push(const T& x) {
    std::unique_lock<std::shared_timed_mutex> l(m_);
    xs_.push(x);
    data_available_.notify_one();
  }

  int size() {
    // TODO: This should really be a shared lock
    // std::unique_lock<std::mutex> l(m_);
    std::shared_lock<std::shared_timed_mutex> l(m_);
    return xs_.size();
  }

  /// Block until the queue contains at least one element, then return the
  /// first element in the queue.
  T pop() {
    std::unique_lock<std::shared_timed_mutex> l(m_);
    while (xs_.size() == 0) {
      data_available_.wait(l);
    }
    const T x = xs_.front();
    xs_.pop();
    return x;
  }

  boost::optional<T> try_pop() {
    std::unique_lock<std::shared_timed_mutex> l(m_);
    if (xs_.size() > 0) {
      const T x = xs_.front();
      xs_.pop();
      return x;
    } else {
      return {};
    }
  }
  
  std::vector<T> try_pop_batch(size_t batch_size) {
    std::unique_lock<std::shared_timed_mutex> l(m_);
    std::vector<T> batch;
    while (xs_.size() > 0 && batch.size() < batch_size) {
      batch.push_back(xs_.front());
      xs_.pop();
    }
    return batch;
  }

  void clear() {
    std::unique_lock<std::shared_timed_mutex> l(m_);
    xs_.clear();
  }

 private:
  std::shared_timed_mutex m_;
  std::condition_variable_any data_available_;
  std::queue<T> xs_;
};

}  // namespace clipper
#endif
