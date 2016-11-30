#include <gtest/gtest.h>
#include <chrono>
#include <thread>

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
  ASSERT_FALSE(timer_future.is_ready());
  ASSERT_EQ(ts_.num_outstanding_timers(), (size_t)1);
  ts_.clock_.increment(10000);
  ASSERT_FALSE(timer_future.is_ready());
  ts_.clock_.increment(11000);
  // Let the timer system detect this
  std::this_thread::sleep_for(500us);
  ASSERT_TRUE(timer_future.is_ready()) << "Uh oh";
}

TEST_F(TimerSystemTests, TwoTimerExpire) {
  auto t20 = ts_.set_timer(20000);
  auto t10 = ts_.set_timer(10000);
  ASSERT_EQ(ts_.num_outstanding_timers(), (size_t)2);
  ASSERT_FALSE(t20.is_ready());
  ASSERT_FALSE(t10.is_ready());
  ts_.clock_.increment(10000);
  std::this_thread::sleep_for(500us);
  ASSERT_FALSE(t20.is_ready());
  ASSERT_TRUE(t10.is_ready());
  ts_.clock_.increment(10000);
  std::this_thread::sleep_for(500us);
  ASSERT_TRUE(t20.is_ready());
  ASSERT_TRUE(t10.is_ready());
}

TEST_F(TimerSystemTests, OutOfOrderTimerExpire) {
  auto t1 = ts_.set_timer(20000);
  auto t2 = ts_.set_timer(10000);
  ASSERT_EQ(ts_.num_outstanding_timers(), (size_t)2);
  ts_.clock_.increment(5000);
  auto t3 = ts_.set_timer(10000);
  ASSERT_FALSE(t1.is_ready());
  ASSERT_FALSE(t2.is_ready());
  ASSERT_FALSE(t3.is_ready());
  ASSERT_EQ(ts_.num_outstanding_timers(), (size_t)3);
  ts_.clock_.increment(5000);
  std::this_thread::sleep_for(500us);
  ASSERT_FALSE(t1.is_ready());
  ASSERT_TRUE(t2.is_ready());
  ASSERT_FALSE(t3.is_ready());
  ASSERT_EQ(ts_.num_outstanding_timers(), (size_t)2);
  ts_.clock_.increment(5000);
  std::this_thread::sleep_for(500us);
  ASSERT_FALSE(t1.is_ready());
  ASSERT_TRUE(t2.is_ready());
  ASSERT_TRUE(t3.is_ready());
  ASSERT_EQ(ts_.num_outstanding_timers(), (size_t)1);
  ts_.clock_.increment(5000);
  std::this_thread::sleep_for(500us);
  ASSERT_TRUE(t1.is_ready());
  ASSERT_TRUE(t2.is_ready());
  ASSERT_TRUE(t3.is_ready());
  ASSERT_EQ(ts_.num_outstanding_timers(), (size_t)0);
}

}  // namespace

// int main(int argc, char** argv) {
//   ::testing::InitGoogleTest(&argc, argv);
//   return RUN_ALL_TESTS();
// }
