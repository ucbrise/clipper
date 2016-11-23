#ifndef CLIPPER_LIB_TIMERS_H
#define CLIPPER_LIB_TIMERS_H

// #include <atomic>
// #include <string>
// #include <tuple>
// #include <utility>
#include <chrono>
#include <queue>

#include <boost/thread.hpp>

namespace clipper {

class HighPrecisionClock {
 public:
  HighPrecisionClock() = default;
  HighPrecisionClock(const HighPrecisionClock &) = default;
  HighPrecisionClock &operator=(const HighPrecisionClock &) = default;

  HighPrecisionClock(HighPrecisionClock &&) = default;
  HighPrecisionClock &operator=(HighPrecisionClock &&) = default;

  std::chrono::time_point<std::chrono::high_resolution_clock> now() const {
    return std::chrono::high_resolution_clock::now();
  }
};

class ManualClock {
 public:
  ManualClock()
      : now_{std::chrono::time_point<
            std::chrono::high_resolution_clock>::min()} {}

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

  void expire();

  std::chrono::time_point<std::chrono::high_resolution_clock> deadline_;

 private:
  boost::promise<void> completion_promise_;
};

struct TimerCompare {
  bool operator()(const std::shared_ptr<Timer> &lhs,
                  const std::shared_ptr<Timer> &rhs) const {
    return *lhs < *rhs;
  }
};

// need to use pointers here to get reference semantics
using TimerPQueue =
    std::priority_queue<std::shared_ptr<Timer>,
                        std::vector<std::shared_ptr<Timer>>, TimerCompare>;

// template <typename Clock>
// void manage_timers(TimerPQueue &timers, std::mutex &queue_mutex,
//                   const bool &shutdown, const Clock& c) {
//  std::cout << "In timer event loop" << std::endl;
//  while (!shutdown) {
//    // wait for next timer to expire
//    //    auto cur_time = high_resolution_clock::now();
//    auto cur_time = c.now();
//    std::unique_lock<std::mutex> l(queue_mutex);
//    if (timers.size() > 0) {
//      //      std::cout << "Found " << timers.size() << " timers" <<
//      std::endl;
//      auto earliest_timer = timers.top();
//      auto duration_ms =
//      std::chrono::duration_cast<std::chrono::milliseconds>(
//          earliest_timer->deadline_ - cur_time);
//      if (duration_ms.count() <= 0) {
//        earliest_timer->expire();
//        timers.pop();
//      }
//    }
//  }
//}

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
    // TODO: probably don't want to just detach thread here
    boost::thread(&TimerSystem::manage_timers, this).detach();
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
        //      std::cout << "Found " << timers.size() << " timers" <<
        //      std::endl;
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

  void shutdown() { shutdown_ = true; }

  boost::future<void> set_timer(long duration_micros) {
    assert(initialized_);
    boost::promise<void> promise;
    auto f = promise.get_future();
    auto tp = clock_.now() + std::chrono::microseconds(duration_micros);
    //  Timer timer{tp, promise};
    queue_.emplace(std::make_shared<Timer>(tp, std::move(promise)));
    return f;
  }

  Clock clock_;

 private:
  bool shutdown_ = false;
  bool initialized_ = false;
  std::mutex queue_mutex_;
  TimerPQueue queue_;
};

}  // namespace clipper

#endif
