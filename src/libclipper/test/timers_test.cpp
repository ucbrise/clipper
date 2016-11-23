#include <gtest/gtest.h>

#include <clipper/timers.hpp>

using namespace clipper;

class TimerSystemTests : public ::testing::Test {
 public:
  TimerSystem<ManualClock> ts_{ManualClock()};
};

TEST_F(TimerSystemTests, SingleTimerExpire) {
  auto timer_future = ts_.set_timer(20000);
  ASSERT_FALSE(timer_future.is_ready());
  ts_.clock_.increment(10000);
  ASSERT_FALSE(timer_future.is_ready());
  ts_.clock_.increment(10000);
  ASSERT_TRUE(timer_future.is_ready());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
