#include <gtest/gtest.h>
#include <chrono>
#include <random>
#include <thread>
#include <unordered_map>

#include <clipper/timers.hpp>

using namespace clipper;
using namespace std::chrono_literals;

namespace {

class TimerSystemTests : public ::testing::Test {
 public:
  TimerSystem<ManualClock> ts_{ManualClock()};
};

TEST_F(TimerSystemTests, SingleTimerExpire) {
  ASSERT_EQ(ts_.num_outstanding_timers(), (size_t)0);
  auto timer_future = ts_.set_timer(20000);
  ASSERT_FALSE(timer_future.isReady());
  ASSERT_EQ(ts_.num_outstanding_timers(), (size_t)1);
  ts_.clock_.increment(10000);
  ASSERT_FALSE(timer_future.isReady());
  ts_.clock_.increment(11000);
  // Let the timer system detect this
  std::this_thread::sleep_for(1000us);
  ASSERT_TRUE(timer_future.isReady()) << "Uh oh";
}

TEST_F(TimerSystemTests, TwoTimerExpire) {
  auto t20 = ts_.set_timer(20000);
  auto t10 = ts_.set_timer(10000);
  ASSERT_EQ(ts_.num_outstanding_timers(), (size_t)2);
  ASSERT_FALSE(t20.isReady());
  ASSERT_FALSE(t10.isReady());
  ts_.clock_.increment(10000);
  std::this_thread::sleep_for(1000us);
  ASSERT_FALSE(t20.isReady());
  ASSERT_TRUE(t10.isReady());
  ts_.clock_.increment(10000);
  std::this_thread::sleep_for(1000us);
  ASSERT_TRUE(t20.isReady());
  ASSERT_TRUE(t10.isReady());
}

TEST_F(TimerSystemTests, OutOfOrderTimerExpire) {
  auto t1 = ts_.set_timer(20000);
  auto t2 = ts_.set_timer(10000);
  ASSERT_EQ(ts_.num_outstanding_timers(), (size_t)2);
  ts_.clock_.increment(5000);
  auto t3 = ts_.set_timer(10000);
  ASSERT_FALSE(t1.isReady());
  ASSERT_FALSE(t2.isReady());
  ASSERT_FALSE(t3.isReady());
  ASSERT_EQ(ts_.num_outstanding_timers(), (size_t)3);
  ts_.clock_.increment(5000);
  std::this_thread::sleep_for(1000us);
  ASSERT_FALSE(t1.isReady());
  ASSERT_TRUE(t2.isReady());
  ASSERT_FALSE(t3.isReady());
  ASSERT_EQ(ts_.num_outstanding_timers(), (size_t)2);
  ts_.clock_.increment(5000);
  std::this_thread::sleep_for(1000us);
  ASSERT_FALSE(t1.isReady());
  ASSERT_TRUE(t2.isReady());
  ASSERT_TRUE(t3.isReady());
  ASSERT_EQ(ts_.num_outstanding_timers(), (size_t)1);
  ts_.clock_.increment(5000);
  std::this_thread::sleep_for(1000us);
  ASSERT_TRUE(t1.isReady());
  ASSERT_TRUE(t2.isReady());
  ASSERT_TRUE(t3.isReady());
  ASSERT_EQ(ts_.num_outstanding_timers(), (size_t)0);
}

TEST_F(TimerSystemTests, ManyTimers) {
  std::unordered_map<int, folly::Future<folly::Unit>> created_timers;
  int max_time = 100000;
  std::random_device rd;
  std::mt19937 generator(rd());
  std::uniform_int_distribution<> dist(0, max_time);
  int num_timers = 10000;
  for (int i = 0; i < num_timers; ++i) {
    int expiration_time = dist(generator);
    created_timers.emplace(expiration_time, ts_.set_timer(expiration_time));
  }
  int time_increment = 10;
  for (int cur_time = 0; cur_time <= max_time; cur_time += time_increment) {
    ts_.clock_.increment(time_increment);
    std::this_thread::sleep_for(100us);
    for (auto t = created_timers.begin(); t != created_timers.end();) {
      if (t->first <= cur_time) {
        ASSERT_TRUE(t->second.isReady());
        t = created_timers.erase(t);
      } else {
        ++t;
      }
    }
  }
  ASSERT_EQ(created_timers.size(), (size_t)0);
}

}  // namespace
