#include <gtest/gtest.h>

#include <clipper/constants.hpp>

int main(int argc, char** argv) {
  clipper::Config& conf = clipper::get_config();
  conf.set_redis_port(clipper::REDIS_TEST_PORT);
  conf.ready();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
