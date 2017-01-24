#ifndef CLIPPER_LOGGING_HPP
#define CLIPPER_LOGGING_HPP

#include <string>
#include <sstream>

#include <spdlog/spdlog.h>

namespace clipper {

static constexpr uint LOGGING_FORMAT_LENGTH = 18;
static constexpr uint MAX_TAG_LENGTH = 10;
static const std::string LOGGER_NAME = "clipper";

class Logger {
 public:
  Logger();
  static Logger &get();

  template<class ...Strings>
  void log_info(const std::string tag, Strings... messages) const;
  template<class ...Args>
  void log_info_formatted(const std::string tag, const char *message, Args... args) const;
  template<class ...Strings>
  void log_debug(const std::string tag, Strings...messages) const;
  template<class ...Args>
  void log_debug_formatted(const std::string tag, const char *message, Args... args) const;
  template<class ...Strings>
  void log_error(const std::string tag, Strings...messages) const;
  template<class ...Args>
  void log_error_formatted(const std::string tag, const char *message, Args... args) const;

 private:
  template<class T, class ...Rest>
  void concatenate_messages(std::stringstream &ss,
                            size_t tag_length,
                            bool first_message,
                            T message,
                            Rest... messages) const;
  template<class T>
  void concatenate_messages(std::stringstream &ss, size_t tag_length, bool first_message, T message) const;
  const std::string get_formatted_tag(const std::string tag) const;

  std::shared_ptr<spdlog::logger> spdlogger_;

};

template<class ...Strings>
void Logger::log_info(const std::string tag, Strings...messages) const {
  const std::string tag_string = get_formatted_tag(tag);
  std::stringstream ss;
  concatenate_messages(ss, tag_string.length(), true, messages...);
  spdlogger_->info((tag_string + ss.str()).data());
}

template<class ...Args>
void Logger::log_info_formatted(const std::string tag, const char *message, Args... args) const {
  const std::string tag_string = get_formatted_tag(tag);
  spdlogger_->info((tag_string + message).data(), args...);
}

template<class ...Strings>
void Logger::log_debug(const std::string tag, Strings...messages) const {
  const std::string tag_string = get_formatted_tag(tag);
  std::stringstream ss;
  concatenate_messages(ss, tag_string.length(), true, messages...);
  spdlogger_->debug((tag_string + ss.str()).data());
}

template<class ...Args>
void Logger::log_debug_formatted(const std::string tag, const char *message, Args... args) const {
  const std::string tag_string = get_formatted_tag(tag);
  spdlogger_->debug((tag_string + message).data(), args...);
}

template<class ...Strings>
void Logger::log_error(const std::string tag, Strings...messages) const {
  const std::string tag_string = get_formatted_tag(tag);
  std::stringstream ss;
  concatenate_messages(ss, tag_string.length(), true, messages...);
  spdlogger_->error((tag_string + ss.str()).data());
}

template<class ...Args>
void Logger::log_error_formatted(const std::string tag, const char *message, Args... args) const {
  const std::string tag_string = get_formatted_tag(tag);
  spdlogger_->error((tag_string + message).data(), args...);
}

template<class T, class ...Rest>
void Logger::concatenate_messages(std::stringstream &ss,
                                  size_t tag_length,
                                  bool first_message,
                                  T message,
                                  Rest... messages) const {
  if (!first_message) {
    ss << std::string(LOGGING_FORMAT_LENGTH + tag_length, ' ');
  }
  ss << message;
  ss << std::endl;
  concatenate_messages(ss, tag_length, false, messages...);
}

template<class T>
void Logger::concatenate_messages(std::stringstream &ss, size_t tag_length, bool first_message, T message) const {
  if (!first_message) {
    ss << std::string(LOGGING_FORMAT_LENGTH + tag_length, ' ');
  }
  ss << message;
}

}

#endif //CLIPPER_LOGGING_HPP
