#include <gtest/gtest.h>

#include <sstream>

#include <clipper/logging.hpp>

using namespace clipper;

namespace {

TEST(LoggingTests, LogSomething) {
  std::ostringstream oss;
  Logger logger(oss);
  logger.log_error("FAIL", "FAILS");
  std::cout << oss.str() << std::endl;
}


} // namespace