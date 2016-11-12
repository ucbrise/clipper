#ifndef CLIPPER_LIB_TIMERS_H
#define CLIPPER_LIB_TIMERS_H

// #include <atomic>
// #include <string>
// #include <tuple>
// #include <utility>
#include <chrono>
#include <queue>

#define BOOST_THREAD_VERSION 3
#include <boost/thread.hpp>

namespace clipper {

class Timer {
 public:
  Timer() = delete;
  Timer(std::chrono::time_point<std::chrono::high_resolution_clock> deadline,
        boost::promise<void> completion_promise);
  ~Timer() = default;

  Timer(const Timer &) = default;
  Timer &operator=(const Timer &) = default;
  Timer(Timer &&) = default;
  Timer &operator=(Timer &&) = default;

  const bool operator<(const Timer &rhs) const;

  void expire();

  std::chrono::time_point<std::chrono::high_resolution_clock> deadline_;

 private:
  boost::promise<void> completion_promise_;

};

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
  std::priority_queue<Timer> queue_;
};

}  // namespace clipper

#endif
