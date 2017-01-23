#ifndef CLIPPER_LOGGING_HPP
#define CLIPPER_LOGGING_HPP

#include <string>

#include <spdlog/spdlog.h>

namespace clipper {

static const std::string LOGGER_NAME = "clipper";

class Logger {
 public:
  Logger();
  static Logger& get_logger();

  void log_info(const std::string message) const;
  void log_debug(const std::string message) const;
  void log_error(const std::string message) const;
 private:
  std::shared_ptr<spdlog::logger> spdlogger_;

};


}

#endif //CLIPPER_LOGGING_HPP
