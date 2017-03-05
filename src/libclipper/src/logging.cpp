#include <spdlog/sinks/ostream_sink.h>
#include <spdlog/spdlog.h>
#include <boost/algorithm/string.hpp>

#include <clipper/logging.hpp>

namespace clipper {

Logger::Logger() {
  // Establishes asynchronous logging and defines an
  // asynchronous logging qeueue with a capacity of 8192 elements
  spdlog::set_async_mode(8192, spdlog::async_overflow_policy::block_retry);
  spdlogger_ = spdlog::stdout_color_mt(LOGGER_NAME);
  spdlogger_->set_pattern(LOGGING_FORMAT);
}

Logger::Logger(std::ostringstream &output_stream) {
  // Establishes asynchronous logging and defines an
  // asynchronous logging qeueue with a capacity of 8192 elements
  spdlog::set_async_mode(8192, spdlog::async_overflow_policy::block_retry);
  auto oss_sink =
      std::make_shared<spdlog::sinks::ostream_sink_mt>(output_stream);
  spdlogger_ = std::make_shared<spdlog::logger>(LOGGER_NAME, oss_sink);
  spdlogger_->set_pattern(LOGGING_FORMAT);
}

Logger &Logger::get() {
  // References a global singleton Logger object.
  // This object is created if it does not already exist,
  // and it is automatically memory managed
  static Logger instance;
  return instance;
}

const std::string Logger::get_formatted_tag(const std::string tag,
                                            LogLevel log_level) const {
  std::string tag_string(tag);
  boost::to_upper(tag_string);
  size_t tag_length = tag_string.length();
  std::stringstream ss;
  size_t level_padding = 0;
  switch (log_level) {
    case LogLevel::Info:
      level_padding =
          MAX_LOGGING_LEVEL_FORMAT_LENGTH - LOGGING_LEVEL_FORMAT_INFO_LENGTH;
      break;
    case LogLevel::Error:
      level_padding =
          MAX_LOGGING_LEVEL_FORMAT_LENGTH - LOGGING_LEVEL_FORMAT_ERROR_LENGTH;
      break;
    case LogLevel::Debug:
      level_padding =
          MAX_LOGGING_LEVEL_FORMAT_LENGTH - LOGGING_LEVEL_FORMAT_DEBUG_LENGTH;
      break;
    default: break;
  }
  ss << std::string(level_padding, ' ');
  if (tag_length < MAX_TAG_LENGTH) {
    ss << std::string(MAX_TAG_LENGTH - tag_length, ' ');
  } else if (tag_length > MAX_TAG_LENGTH) {
    tag_string = tag_string.substr(0, MAX_TAG_LENGTH - 3) + "...";
  }
  ss << "[" << tag_string << "] ";
  return ss.str();
}

void Logger::pad_logging_stream_for_alignment(std::stringstream &ss,
                                              LogLevel log_level,
                                              size_t tag_length) const {
  ss << std::string(LOGGING_FORMAT_LENGTH + tag_length, ' ');
  size_t log_padding = 0;
  switch (log_level) {
    case LogLevel::Info: log_padding = LOGGING_LEVEL_FORMAT_INFO_LENGTH; break;
    case LogLevel::Error:
      log_padding = LOGGING_LEVEL_FORMAT_ERROR_LENGTH;
      break;
    case LogLevel::Debug:
      log_padding = LOGGING_LEVEL_FORMAT_DEBUG_LENGTH;
      break;
    default: break;
  }
  ss << std::string(log_padding, ' ');
}

}  // namespace clipper