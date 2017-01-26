#ifndef CLIPPER_LOGGING_HPP
#define CLIPPER_LOGGING_HPP

#include <string>
#include <sstream>

#include <spdlog/spdlog.h>

namespace clipper {

static constexpr uint LOGGING_FORMAT_LENGTH = 18;
static constexpr uint MAX_TAG_LENGTH = 10;
// Defines the logging format as [HH:MM:SS.mmm][<LOG_LEVEL_1_CHAR>] <Message>
// Note: <Message> includes a formatted, user-defined tag (see get_formatted_tag())
static const std::string LOGGING_FORMAT = "[%T.%e][%L] %v";
static const std::string LOGGER_NAME = "clipper";

class Logger {
 public:
  /**
   * Constructs a logger that will write all output
   * to the specified output stream. To be used for
   * testing!
   */
  explicit Logger(std::ostringstream& output_stream);
  /**
   * Obtains an instance of the Logger singleton
   * that can be used to log messages at specified levels
   */
  static Logger &get();

  /**
   * Logs one or more string messages at the "info" log level with the specified tag
   */
  template<class ...Strings>
  void log_info(const std::string tag, Strings... messages) const;
  /**
   * Logs a single formatted message at the "info" log level
   * with the specified tag
   *
   * @param args The formatting arguments
   */
  template<class ...Args>
  void log_info_formatted(const std::string tag, const char *message, Args... args) const;
  /**
   * Logs one or more string messages at the "debug" log level with the specified tag
   */
  template<class ...Strings>
  void log_debug(const std::string tag, Strings...messages) const;
  template<class ...Args>
  /**
   * Logs a single formatted message at the "debug" log level
   * with the specified tag
   *
   * @param args The formatting arguments
   */
  void log_debug_formatted(const std::string tag, const char *message, Args... args) const;
  template<class ...Strings>
  /**
   * Logs one or more string messages at the "error" log level with the specified tag
   */
  void log_error(const std::string tag, Strings...messages) const;
  template<class ...Args>
  /**
   * Logs a single formatted message at the "error" log level
   * with the specified tag
   *
   * @param args The formatting arguments
   */
  void log_error_formatted(const std::string tag, const char *message, Args... args) const;

 private:
  Logger();
  /**
   * Concatenates multiple log messages into a single message, where
   * individual messages are separated by "newlines" and padded based
   * on other log attributes (tag length and format length) to achieve
   * even spacing and alignment. This is a recursive function that makes
   * use of variadic templates.
   */
  template<class T, class ...Rest>
  void concatenate_messages(std::stringstream &ss,
                            size_t tag_length,
                            bool first_message,
                            T message,
                            Rest... messages) const;
  /**
   * The base case for concatenating multiple log messages into a single message.
   * For more information, explore recursion with variadic template functions.
   */
  template<class T>
  void concatenate_messages(std::stringstream &ss, size_t tag_length, bool first_message, T message) const;
  /**
   * Given tag text for a log message, creates a formated tag of the form
   * <padding>[<tag_text>], where <padding is a series of space characters
   * used to ensure that all formatted tags are of the same length, dependent
   * upon MAX_TAG_LENGTH. tags with text exceeding MAX_TAG_LENGTH are
   * truncated and ellipsized during formatting.
   */
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

} // namespace clipper

#endif //CLIPPER_LOGGING_HPP
