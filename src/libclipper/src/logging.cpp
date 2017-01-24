#include <spdlog/spdlog.h>
#include <boost/algorithm/string.hpp>

#include <clipper/logging.hpp>
#include <sstream>
#include <iostream>

namespace clipper {

  Logger::Logger() {
    spdlog::set_async_mode(8192, spdlog::async_overflow_policy::block_retry);
    spdlogger_ = spdlog::stdout_color_mt(LOGGER_NAME);
    spdlogger_->set_pattern("[%T.%e][%L] %v");
  }

  Logger& Logger::get() {
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

}