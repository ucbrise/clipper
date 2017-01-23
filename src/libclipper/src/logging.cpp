#include <spdlog/spdlog.h>

#include <clipper/logging.hpp>

namespace clipper {

  Logger::Logger() {
    spdlog::set_async_mode(8192, spdlog::async_overflow_policy::block_retry);
    spdlogger_ = spdlog::stdout_color_mt(LOGGER_NAME);
  }

  Logger& Logger::get_logger() {
    static Logger instance;
    return instance;
  }

  void Logger::log_info(const std::string message) const {
    spdlogger_->info(message);
  }

  void Logger::log_debug(const std::string message) const {
    spdlogger_->debug(message);
  }

  void Logger::log_error(const std::string message) const {
    spdlogger_->error(message);
  }

}