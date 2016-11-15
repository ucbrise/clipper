
#include <cassert>
#include <chrono>
#include <queue>
#include <thread>
#include <utility>

#include <boost/thread.hpp>
#include <clipper/timers.hpp>
#include <clipper/util.hpp>

using std::pair;
using std::chrono::high_resolution_clock;

namespace clipper {

void manage_timers(TimerPQueue &timers, std::mutex &queue_mutex,
                   const bool &shutdown) {
  std::cout << "In timer event loop" << std::endl;
  while (!shutdown) {
    // wait for next timer to expire
    auto cur_time = high_resolution_clock::now();
    std::unique_lock<std::mutex> l(queue_mutex);
    if (timers.size() > 0) {
      //      std::cout << "Found " << timers.size() << " timers" << std::endl;
      auto earliest_timer = timers.top();
      auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
          earliest_timer->deadline_ - cur_time);
      if (duration_ms.count() <= 0) {
        earliest_timer->expire();
        timers.pop();
      }
    }
  }
}

Timer::Timer(
    std::chrono::time_point<std::chrono::high_resolution_clock> deadline,
    boost::promise<void> completion_promise)
    : deadline_(deadline), completion_promise_(std::move(completion_promise)) {}

bool Timer::operator<(const Timer &rhs) const {
  return deadline_ < rhs.deadline_;
}

void Timer::expire() {
  std::cout << "TIMER EXPIRED IN TIMER THREAD: " << std::this_thread::get_id()
            << std::endl;
  completion_promise_.set_value();
}

TimerSystem::TimerSystem() : queue_(TimerPQueue{}) {
  std::cout << "starting timer thread" << std::endl;

  start();
  std::cout << "timer thread started" << std::endl;
}

void TimerSystem::start() {
  // TODO: probably don't want to just detach thread here
  boost::thread(&manage_timers, boost::ref(queue_), boost::ref(queue_mutex_),
                boost::ref(shutdown_))
      .detach();
  initialized_ = true;
}

void TimerSystem::shutdown() { shutdown_ = true; }

TimerSystem::~TimerSystem() { shutdown(); }

boost::future<void> TimerSystem::set_timer(long duration_micros) {
  assert(initialized_);
  boost::promise<void> promise;
  auto f = promise.get_future();
  auto tp = std::chrono::high_resolution_clock::now() +
            std::chrono::microseconds(duration_micros);
  //  Timer timer{tp, promise};
  queue_.emplace(std::make_shared<Timer>(tp, std::move(promise)));
  return f;
}

// using TimerId = long;

}  // namespace clipper
