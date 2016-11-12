#ifndef CLIPPER_LIB_UTIL_H
#define CLIPPER_LIB_UTIL_H

#include <condition_variable>
#include <mutex>
#include <vector>
// uncomment to disable assert()
// #define NDEBUG
#include <cassert>

#include "boost/optional.hpp"

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
    std::unique_lock<std::mutex> l(m_);
    xs_.push_back(x);
    data_available_.notify_one();
  }

  int size() const {
    // TODO: This should really be a shared lock
    std::unique_lock<std::mutex> l(m_);
    return xs_.size();
  }

  /// Block until the queue contains at least one element, then return the
  /// first element in the queue.
  T pop() {
    std::unique_lock<std::mutex> l(m_);
    while (xs_.size() == 0) {
      data_available_.wait(l);
    }
    const T x = xs_.front();
    xs_.erase(std::begin(xs_));
    return x;
  }

  boost::optional<T> try_pop() {
    std::unique_lock<std::mutex> l(m_);
    if (xs_.size() > 0) {
      const T x = xs_.front();
      xs_.erase(std::begin(xs_));
      return x;
    } else {
      return {};
    }
  }

  /// pops up to batch_size elements from the front of the queue.
  /// If the batch size is larger than the size of the queue,
  /// all elements will be removed from the queue. This method never blocks.
  std::vector<T> try_pop_batch(int batch_size) {
    std::unique_lock<std::mutex> l(m_);
    if (xs_.size() >= batch_size) {
      const std::vector<T> batch(xs_.begin(), xs_.begin() + batch_size);
      xs_.erase(xs_.begin(), xs_.begin() + batch_size);
      return batch;
    } else if (xs_.size() > 0) {
      std::vector<T> batch;
      batch.swap(xs_);
      assert(xs_.size() == 0);
      return batch;
    } else {
      return {};
    }
  }

  void clear() {
    std::unique_lock<std::mutex> l(m_);
    xs_.clear();
  }

 private:
  std::mutex m_;
  std::condition_variable data_available_;
  std::vector<T> xs_;
};

}  // namespace clipper
#endif
