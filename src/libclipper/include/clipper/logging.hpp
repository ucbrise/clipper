#ifndef CLIPPER_LOGGING_HPP
#define CLIPPER_LOGGING_HPP

#include <string>
#include <sstream>

#include <spdlog/spdlog.h>

namespace clipper {

static const std::string LOGGER_NAME = "clipper";

class Logger {
 public:
  Logger();
  static Logger &get();

  template<class ...Strings>
  void log_info(Strings... messages) const;
  template<class ...Strings>
  void log_debug(Strings...messages) const;
  template<class ...Strings>
  void log_error(Strings...messages) const;

 private:
  template<class T, class ...Rest>
  void concatenate_messages(std::stringstream &ss, T first_message, Rest... messages) const;
  template<class T>
  void concatenate_messages(std::stringstream &ss, T message) const;
  template<class T, class ...Rest>
  void log_messages(T first_message, Rest... messages, std::function<void(T)> logging_function);
  template<class T>
  void log_messages(T message, std::function<void(T)> logging_function);

  std::shared_ptr<spdlog::logger> spdlogger_;

};

template<class ...Strings>
void Logger::log_info(Strings...messages) const {
  std::stringstream ss;
  concatenate_messages(ss, messages...);
  spdlogger_->info(ss.str());
}

template<class ...Strings>
void Logger::log_debug(Strings...messages) const {
  std::stringstream ss;
  concatenate_messages(ss, messages...);
  spdlogger_->debug(ss.str());
}

template<class ...Strings>
void Logger::log_error(Strings...messages) const {
  std::stringstream ss;
  concatenate_messages(ss, messages...);
  spdlogger_->error(ss.str());
}

template<class T, class ...Rest>
void Logger::log_messages(T first_message, Rest... messages, std::function<void(T)> logging_function) {
  logging_function(message);
  log_messages(messages..., logging_function);
};

template<class T>
void log_messages(T message, std::function<void(T)> logging_function) {
  logging_function(message);
}

template<class T, class ...Rest>
void Logger::concatenate_messages(std::stringstream &ss, T first_message, Rest... messages) const {
  ss << first_message;
  ss << std::endl;
  concatenate_messages(ss, messages...);
}

template<class T>
void Logger::concatenate_messages(std::stringstream &ss, T message) const {
  ss << message;
}

}

#endif //CLIPPER_LOGGING_HPP
