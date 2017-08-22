#include <gtest/gtest.h>

#include <chrono>
#include <stdexcept>
#include <thread>

#include <clipper/config.hpp>

using namespace clipper;

class ConfigTest : public ::testing::Test {
 public:
  ConfigTest() { get_config().reset(); }
};

TEST_F(ConfigTest, TestChangeValues) {
  Config& conf1 = get_config();
  ASSERT_FALSE(conf1.is_readable());
  conf1.set_redis_address("test_address");
  conf1.set_redis_port(1234);
  conf1.set_prediction_cache_size(16);
  conf1.ready();
  ASSERT_EQ(conf1.get_redis_address(), "test_address");
  ASSERT_EQ(conf1.get_redis_port(), 1234);
  ASSERT_EQ(conf1.get_prediction_cache_size(), 16);

  Config& conf2 = get_config();
  ASSERT_EQ(conf2.get_redis_address(), "test_address");
  ASSERT_EQ(conf2.get_redis_port(), 1234);
  ASSERT_EQ(conf2.get_prediction_cache_size(), 16);
}

TEST_F(ConfigTest, TestReadBeforeReady) {
  Config& conf = get_config();
  ASSERT_THROW(conf.get_redis_port(), std::logic_error);
  ASSERT_THROW(conf.get_redis_address(), std::logic_error);
  ASSERT_THROW(conf.get_prediction_cache_size(), std::logic_error);
}

TEST_F(ConfigTest, TestWriteAfterReady) {
  Config& conf = get_config();
  conf.ready();
  ASSERT_THROW(conf.set_redis_port(5555), std::logic_error);
  ASSERT_THROW(conf.set_redis_address("new_address"), std::logic_error);
  ASSERT_THROW(conf.set_prediction_cache_size(32), std::logic_error);
}

TEST_F(ConfigTest, TestReadManyThreads) {
  Config& conf1 = get_config();
  conf1.set_redis_address("test_address");
  conf1.set_redis_port(1234);
  conf1.set_prediction_cache_size(16);
  conf1.ready();
  std::vector<std::thread> threads;
  for (int i = 0; i < 100; ++i) {
    threads.push_back(std::thread([i]() {
      // We mod by 5 so that 5 threads all read at the same time to
      // get closer to concurrent access (which should be safe!)
      int sleep_time = (i / 5) * 20;
      std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
      Config& conf = get_config();
      ASSERT_EQ(conf.get_redis_address(), "test_address");
      ASSERT_EQ(conf.get_redis_port(), 1234);
      ASSERT_EQ(conf.get_prediction_cache_size(), 16);
      ASSERT_THROW(conf.set_redis_port(5555), std::logic_error);
      ASSERT_THROW(conf.set_redis_address("new_address"), std::logic_error);
      ASSERT_THROW(conf.set_prediction_cache_size(16), std::logic_error);
    }));
  }
  for (std::thread& t : threads) {
    t.join();
  }
}

TEST_F(ConfigTest, TestSpecifyingNegativePredictionCacheSizeThrowsError) {
  Config& conf = get_config();
  ASSERT_THROW(conf.set_prediction_cache_size(-1), std::invalid_argument);
}
