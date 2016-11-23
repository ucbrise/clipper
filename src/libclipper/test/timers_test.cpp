#include <gtest/gtest.h>
#include <chrono>
#include <thread>

#include <clipper/timers.hpp>

using namespace clipper;
using namespace std::chrono_literals;

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

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
