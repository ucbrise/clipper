#ifndef CLIPPER_LIB_TIMERS_H
#define CLIPPER_LIB_TIMERS_H

// #include <atomic>
// #include <string>
// #include <tuple>
// #include <utility>
#include <chrono>
#include <queue>

#define BOOST_THREAD_VERSION 4
#include <boost/thread.hpp>

namespace clipper {

class Timer {
 public:
  Timer() = delete;
  Timer(std::chrono::time_point<std::chrono::high_resolution_clock> deadline,
        boost::promise<void> completion_promise);
  ~Timer() = default;

  Timer(const Timer &) = delete;
  Timer &operator=(const Timer &) = delete;
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

class TimerSystem {
 public:
  TimerSystem();

  ~TimerSystem();

  void start();

  void shutdown();

  boost::future<void> set_timer(long duration_micros);

 private:
  bool shutdown_ = false;
  bool initialized_ = false;
  std::mutex queue_mutex_;
  TimerPQueue queue_;
};

}  // namespace clipper

#endif
