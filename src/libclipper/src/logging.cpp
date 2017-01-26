#include <spdlog/spdlog.h>
#include <spdlog/sinks/ostream_sink.h>
#include <boost/algorithm/string.hpp>

#include <clipper/logging.hpp>

namespace clipper {

  Logger::Logger() {
    spdlog::set_async_mode(8192, spdlog::async_overflow_policy::block_retry);
    spdlogger_ = spdlog::stdout_color_mt(LOGGER_NAME);
    spdlogger_->set_pattern(LOGGING_FORMAT);
  }

  Logger::Logger(std::ostringstream &output_stream) {
    spdlog::set_async_mode(8192, spdlog::async_overflow_policy::block_retry);
    auto oss_sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(output_stream);
    spdlogger_ = std::make_shared<spdlog::logger>(LOGGER_NAME, oss_sink);
    spdlogger_->set_pattern(LOGGING_FORMAT);
  }

  Logger& Logger::get() {
    // References a global singleton Logger object.
    // This object is created if it does not already exist,
    // and it is automatically memory managed
    static Logger instance;
    return instance;
  }

  const std::string Logger::get_formatted_tag(const std::string tag) const {
    std::string tag_string(tag);
    boost::to_upper(tag_string);
    size_t tag_length = tag_string.length();
    std::stringstream ss;
    if(tag_length < MAX_TAG_LENGTH) {
      ss << std::string(MAX_TAG_LENGTH - tag_length, ' ');
    } else if(tag_length > MAX_TAG_LENGTH) {
      tag_string = tag_string.substr(0, MAX_TAG_LENGTH - 3) + "...";
    }
    ss << "[" << tag_string << "] ";
    return ss.str();
  }

} // namespace clipper