#ifndef CLIPPER_LIB_TIMERS_H
#define CLIPPER_LIB_TIMERS_H

#include <atomic>
#include <chrono>
#include <mutex>
#include <queue>
#include <thread>

#include <boost/thread.hpp>

namespace clipper {

class HighPrecisionClock {
 public:
  HighPrecisionClock() = default;
  ~HighPrecisionClock() = default;
  HighPrecisionClock(const HighPrecisionClock &) = default;
  HighPrecisionClock &operator=(const HighPrecisionClock &) = default;

  HighPrecisionClock(HighPrecisionClock &&) = default;
  HighPrecisionClock &operator=(HighPrecisionClock &&) = default;

  std::chrono::time_point<std::chrono::high_resolution_clock> now() const {
    return std::chrono::high_resolution_clock::now();
  }
};

/// Used for unit testing
class ManualClock {
 public:
  ManualClock()
      : now_{std::chrono::time_point<
            std::chrono::high_resolution_clock>::min()} {}

  ManualClock(const ManualClock &other) = default;
  ManualClock &operator=(const ManualClock &other) = default;

  ManualClock(ManualClock &&other) = default;
  ManualClock &operator=(ManualClock &&other) = default;

  ~ManualClock() = default;

  void increment(int increment_micros) {
    assert(increment_micros >= 0);
    now_ += std::chrono::microseconds(increment_micros);
  }

  std::chrono::time_point<std::chrono::high_resolution_clock> now() const {
    return now_;
  }

 private:
  std::chrono::time_point<std::chrono::high_resolution_clock> now_;
};

class Timer {
 public:
  Timer() = delete;
  Timer(std::chrono::time_point<std::chrono::high_resolution_clock> deadline,
        boost::promise<void> completion_promise);
  ~Timer() = default;

  // Disallow copy
  Timer(const Timer &) = delete;
  Timer &operator=(const Timer &) = delete;

  // Move constructors
  Timer(Timer &&) = default;
  Timer &operator=(Timer &&) = default;

  bool operator<(const Timer &rhs) const;
  bool operator>(const Timer &rhs) const;
  bool operator<=(const Timer &rhs) const;
  bool operator>=(const Timer &rhs) const;

  void expire();

  std::chrono::time_point<std::chrono::high_resolution_clock> deadline_;

 private:
  boost::promise<void> completion_promise_;
};

struct TimerCompare {
  bool operator()(const std::shared_ptr<Timer> &lhs,
                  const std::shared_ptr<Timer> &rhs) const {
    return *lhs > *rhs;
    // return *rhs < *lhs;
  }
};

// need to use pointers here to get reference semantics
using TimerPQueue =
    std::priority_queue<std::shared_ptr<Timer>,
                        std::vector<std::shared_ptr<Timer>>, TimerCompare>;

template <typename Clock>
class TimerSystem {
 public:
  explicit TimerSystem(Clock c) : clock_(c), queue_(TimerPQueue{}) {
    std::cout << "starting timer thread" << std::endl;
    start();
    std::cout << "timer thread started" << std::endl;
  }

  ~TimerSystem() { shutdown(); }

  TimerSystem(const TimerSystem &) = delete;
  TimerSystem &operator=(const TimerSystem &) = delete;

  TimerSystem(TimerSystem &&) = default;
  TimerSystem &operator=(TimerSystem &&) = default;

  void start() {
    manager_thread_ = boost::thread(&TimerSystem::manage_timers, this);
    initialized_ = true;
  }

  void manage_timers() {
    std::cout << "In timer event loop" << std::endl;
    while (!shutdown_) {
      // wait for next timer to expire
      //    auto cur_time = high_resolution_clock::now();
      auto cur_time = clock_.now();
      std::unique_lock<std::mutex> l(queue_mutex_);
      if (queue_.size() > 0) {
        auto earliest_timer = queue_.top();
        auto duration_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                earliest_timer->deadline_ - cur_time);
        if (duration_ms.count() <= 0) {
          earliest_timer->expire();
          queue_.pop();
        }
      }
    }
  }

  void shutdown() {
    // signal management thread to shutdown
    shutdown_ = true;
    // wait for it to finish
    manager_thread_.join();
  }

  boost::future<void> set_timer(long duration_micros) {
    assert(initialized_);
    boost::promise<void> promise;
    auto f = promise.get_future();
    auto tp = clock_.now() + std::chrono::microseconds(duration_micros);
    //  Timer timer{tp, promise};
    std::unique_lock<std::mutex> l(queue_mutex_);
    queue_.emplace(std::make_shared<Timer>(tp, std::move(promise)));
    return f;
  }

  size_t num_outstanding_timers() {
    std::unique_lock<std::mutex> l(queue_mutex_);
    return queue_.size();
  }

  Clock clock_;

 private:
  std::atomic<bool> shutdown_{false};
  std::atomic<bool> initialized_{false};
  boost::thread manager_thread_;

  std::mutex queue_mutex_;
  TimerPQueue queue_;
};

}  // namespace clipper

#endif
