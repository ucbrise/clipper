#include <spdlog/spdlog.h>

#include <clipper/logging.hpp>
#include <sstream>
#include <iostream>

namespace clipper {

  Logger::Logger() {
    spdlog::set_async_mode(8192, spdlog::async_overflow_policy::block_retry);
    spdlogger_ = spdlog::stdout_color_mt(LOGGER_NAME);
  }

  Logger& Logger::get() {
    static Logger instance;
    return instance;
  }

}