#include <gtest/gtest.h>
#include <cxxopts.hpp>
#include <iostream>

#include <clipper/config.hpp>

int main(int argc, char** argv) {
  cxxopts::Options options("frontend_tests", "Frontend tests");
  options.add_options()("p,redis_port", "Redis port",
                        cxxopts::value<int>()->default_value("-1"));
  options.parse(argc, argv);
  int redis_port = options["redis_port"].as<int>();
  // means the option wasn't supplied and an exception should be thrown
  if (redis_port == -1) {
    std::cerr << options.help() << std::endl;
    return -1;
  }

  clipper::Config& conf = clipper::get_config();
  conf.set_redis_port(options["redis_port"].as<int>());
  conf.ready();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
