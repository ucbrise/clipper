#include <cassert>
#include <chrono>
#include <queue>
#include <thread>
#include <utility>

#include <boost/thread.hpp>
#include <clipper/timers.hpp>
#include <clipper/util.hpp>

using std::pair;
// using std::chrono::high_resolution_clock;

namespace clipper {

Timer::Timer(
    std::chrono::time_point<std::chrono::high_resolution_clock> deadline,
    boost::promise<void> completion_promise)
    : deadline_(deadline), completion_promise_(std::move(completion_promise)) {}

bool Timer::operator<(const Timer &rhs) const {
  return deadline_ < rhs.deadline_;
}

bool Timer::operator>(const Timer &rhs) const {
  return deadline_ > rhs.deadline_;
}

bool Timer::operator<=(const Timer &rhs) const {
  return deadline_ <= rhs.deadline_;
}

bool Timer::operator>=(const Timer &rhs) const {
  return deadline_ >= rhs.deadline_;
}

void Timer::expire(Query query) {
//  set_q_path_timer_expire(query.test_qid_, std::this_thread::get_id());
//  update_timer_expire_count(std::this_thread::get_id());
//  log_info("qid", query.test_qid_, "in timer expire", std::this_thread::get_id());
  completion_promise_.set_value();
}

}  // namespace clipper
