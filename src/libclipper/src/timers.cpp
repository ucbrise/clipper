
#include <chrono>
#include <queue>
#include <utility>
#include <cassert>

#define BOOST_THREAD_VERSION 3
#include <boost/thread.hpp>
#include <clipper/timers.hpp>
#include <clipper/util.hpp>

using std::pair;
using std::chrono::high_resolution_clock;

namespace clipper {

Timer::Timer(std::chrono::time_point<std::chrono::high_resolution_clock> deadline,
             boost::promise<void> completion_promise)
    : deadline_(deadline), completion_promise_(std::move(completion_promise)) {}

const bool Timer::operator<(const Timer &rhs) const {
  return deadline_ < rhs.deadline_;
}

void Timer::expire() {
  completion_promise_.set_value();
}

TimerSystem::TimerSystem() : queue_(std::priority_queue<Timer, std::vector<Timer>, std::less<Timer>>{}) {
  start();
}

void TimerSystem::start() {
  boost::thread(&manage_timers, boost::ref(queue_), boost::ref(queue_mutex_),
                boost::ref(shutdown_));
  initialized_ = true;
}

TimerSystem::~TimerSystem() { shutdown(); }

boost::future<void> TimerSystem::set_timer(long duration_micros) {
  assert(initialized_);
  boost::promise<void> promise;
  auto f = promise.get_future();
  auto tp = std::chrono::high_resolution_clock::now() + std::chrono::microseconds(duration_micros);
//  Timer timer{tp, promise};
  queue_.emplace(tp, promise);
  return f;
}

// using TimerId = long;
void manage_timers(std::priority_queue<Timer, std::less<Timer>> &timers,
                   std::mutex &queue_mutex, const bool &shutdown) {
  while (!shutdown) {
    // wait for next timer to expire
    auto cur_time = high_resolution_clock::now();
    std::unique_lock<std::mutex> l(queue_mutex);
    if (timers.size() > 0) {
      auto earliest_timer = timers.top().first;
      auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
          earliest_timer - cur_time);
      if (duration_ms.count() <= 0) {
        timers.top().expire();
        timers.pop();
      }
    }
  }
}

}  // namespace clipper
