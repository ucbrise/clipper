#ifndef CLIPPER_LOGGING_HPP
#define CLIPPER_LOGGING_HPP

#include <sstream>
#include <string>

#include <spdlog/spdlog.h>

namespace clipper {

static constexpr uint LOGGING_FORMAT_LENGTH = 17;
static constexpr uint LOGGING_LEVEL_FORMAT_INFO_LENGTH = 4;
static constexpr uint LOGGING_LEVEL_FORMAT_ERROR_LENGTH = 5;
static constexpr uint LOGGING_LEVEL_FORMAT_DEBUG_LENGTH = 5;
static constexpr uint MAX_LOGGING_LEVEL_FORMAT_LENGTH = 5;
static constexpr uint MAX_TAG_LENGTH = 10;

static constexpr int if_task_completed_reserve_value = -1;
static constexpr int task_completed_value = 1;
static constexpr int timer_completed_value = 0;
//  int tid_vec_size = 6;
static constexpr int benchmark_script_tid_index = 0;
static constexpr int if_task_completed_index = 1;
static constexpr int task_or_timer_completion_tid_index = 2;
static constexpr int response_ready_continuation_tid_index = 3;
static constexpr int benchmark_script_continuation_tid_index = 4;
static constexpr int complete_account_index = 5;
static constexpr int incomplete_account_value = 0;
static constexpr int complete_account_value = 1;

static constexpr int BENCH_SCRIPT_INDEX = 0;
static constexpr int WHEN_ANY_INDEX = 1;
static constexpr int TIMER_EXPIRE_INDEX = 2;
static constexpr int RESPONSE_READY_INDEX = 3;
static constexpr int BENCH_CONT_INDEX = 4;

static constexpr int Q_PATH_BENCH_SCRIPT_INDEX = 0;
static constexpr int Q_PATH_TASK_OR_TIMER_INDEX = 1;
static constexpr int Q_PATH_RESPONSE_READY_INDEX = 2;
static constexpr int Q_PATH_BENCH_CONT_INDEX = 3;

static constexpr int COMPLETED_BY_TIMER = 1;
static constexpr int COMPLETED_BY_TASK = 2;
static constexpr int COMPLETED_BY_NEITHER = 3;

//    [count in benchmark, count in when any, count in timer expire,
    // count in response ready, count in bench continuation]


// Defines the logging format as [HH:MM:SS.mmm][<LOG_LEVEL>] <Message>
// Note: <Message> includes a formatted, user-defined tag (see
// get_formatted_tag())
static const std::string LOGGING_FORMAT = "[%T.%e][%l] %v";
static const std::string LOGGER_NAME = "clipper";

enum class LogLevel { Info, Debug, Error };

class Logger {
 public:
  /**
   * Constructs a logger that will write all output
   * to the specified output stream. To be used for
   * testing!
   */
  explicit Logger(std::ostringstream &output_stream);
  /**
   * Obtains an instance of the Logger singleton
   * that can be used to log messages at specified levels
   */
  static Logger &get();

  /**
   * Logs one or more string messages at the "info" log level with the specified
   * tag
   */
  template <class... Strings>
  void log_info(const std::string tag, Strings... messages) const;
  /**
   * Logs a single formatted message at the "info" log level
   * with the specified tag
   *
   * @param args The formatting arguments
   */
  template <class... Args>
  void log_info_formatted(const std::string tag, const char *message,
                          Args... args) const;
  /**
   * Logs one or more string messages at the "debug" log level with the
   * specified tag
   */
  template <class... Strings>
  void log_debug(const std::string tag, Strings... messages) const;
  /**
   * Logs a single formatted message at the "debug" log level
   * with the specified tag
   *
   * @param args The formatting arguments
   */
  template <class... Args>
  void log_debug_formatted(const std::string tag, const char *message,
                           Args... args) const;
  /**
   * Logs one or more string messages at the "error" log level with the
   * specified tag
   */
  template <class... Strings>
  void log_error(const std::string tag, Strings... messages) const;
  /**
   * Logs a single formatted message at the "error" log level
   * with the specified tag
   *
   * @param args The formatting arguments
   */
  template <class... Args>
  void log_error_formatted(const std::string tag, const char *message,
                           Args... args) const;

  // QID -> <[benchmark script TID, timer or when_any expire TID, response
  // ready future continuation TID, benchmark script continuation TID],
  // <which completed, true if response was received>>
  std::unordered_map<int, std::pair<std::vector<std::__thread_id>, std::pair<int, bool>>> q_path_table;

  // TID -> [count in benchmark, count in when any, count in timer expire,
  // count in response ready, count in bench continuation]
  std::unordered_map<std::__thread_id, std::vector<int>> t_counts_table;

  void _update_q_path(int qid, std::__thread_id tid, int index) {
    if (q_path_table.find(qid) == q_path_table.end()) {
      std::__thread_id tid = std::__thread_id();
      std::vector<std::__thread_id> vec = std::vector<std::__thread_id>{tid, tid, tid, tid};
      q_path_table[qid] = std::make_pair(vec, std::make_pair(COMPLETED_BY_NEITHER, false));
    }
    q_path_table[qid].first[index] = tid;
  }

  void _update_q_path_tasks_completed_before_timer(int qid, bool tasks_completed_first) {
    q_path_table[qid].second.first = tasks_completed_first ? COMPLETED_BY_TASK : COMPLETED_BY_TIMER;
  }

  void _update_q_path_response_received(int qid) {
    q_path_table[qid].second.second = true;
  }

  void _update_count(std::__thread_id tid, int index) {
    if (t_counts_table.find(tid) == t_counts_table.end()) {
      t_counts_table[tid] = std::vector<int>{0, 0, 0, 0, 0};
    }

    auto vec = t_counts_table[tid];
    vec[index] += 1;
    t_counts_table[tid] = vec;
  }

 private:
  Logger();
  /**
   * Concatenates multiple log messages into a single message, where
   * individual messages are separated by "newlines" and padded based
   * on other log attributes (tag length and format length) to achieve
   * even spacing and alignment. This is a recursive function that makes
   * use of variadic templates.
   */
  template <class T, class... Rest>
  void concatenate_messages(std::stringstream &ss, LogLevel log_level,
                            size_t tag_length, bool first_message, T message,
                            Rest... messages) const;
  /**
   * The base case for concatenating multiple log messages into a single
   * message.
   * For more information, explore recursion with variadic template functions.
   */
  template <class T>
  void concatenate_messages(std::stringstream &ss, LogLevel log_level,
                            size_t tag_length, bool first_message,
                            T message) const;
  /**
   * Given tag text for a log message, creates a formated tag of the form
   * <padding>[<tag_text>], where <padding is a series of space characters
   * used to ensure that all formatted tags are of the same length, dependent
   * upon MAX_TAG_LENGTH. tags with text exceeding MAX_TAG_LENGTH are
   * truncated and ellipsized during formatting.
   */
  const std::string get_formatted_tag(const std::string tag,
                                      LogLevel log_level) const;

  void pad_logging_stream_for_alignment(std::stringstream &ss,
                                        LogLevel log_level,
                                        size_t tag_length) const;

  std::shared_ptr<spdlog::logger> spdlogger_;
};

template <class... Strings>
void Logger::log_info(const std::string tag, Strings... messages) const {
  const std::string tag_string = get_formatted_tag(tag, LogLevel::Info);
  std::stringstream ss;
  concatenate_messages(ss, LogLevel::Info, tag_string.length(), true,
                       messages...);
  spdlogger_->info((tag_string + ss.str()).data());
}

template <class... Args>
void Logger::log_info_formatted(const std::string tag, const char *message,
                                Args... args) const {
  const std::string tag_string = get_formatted_tag(tag, LogLevel::Info);
  spdlogger_->info((tag_string + message).data(), args...);
}

template <class... Strings>
void Logger::log_debug(const std::string tag, Strings... messages) const {
  const std::string tag_string = get_formatted_tag(tag, LogLevel::Debug);
  std::stringstream ss;
  concatenate_messages(ss, LogLevel::Debug, tag_string.length(), true,
                       messages...);
  spdlogger_->debug((tag_string + ss.str()).data());
}

template <class... Args>
void Logger::log_debug_formatted(const std::string tag, const char *message,
                                 Args... args) const {
  const std::string tag_string = get_formatted_tag(tag, LogLevel::Debug);
  spdlogger_->debug((tag_string + message).data(), args...);
}

template <class... Strings>
void Logger::log_error(const std::string tag, Strings... messages) const {
  const std::string tag_string = get_formatted_tag(tag, LogLevel::Error);
  std::stringstream ss;
  concatenate_messages(ss, LogLevel::Error, tag_string.length(), true,
                       messages...);
  spdlogger_->error((tag_string + ss.str()).data());
}

template <class... Args>
void Logger::log_error_formatted(const std::string tag, const char *message,
                                 Args... args) const {
  const std::string tag_string = get_formatted_tag(tag, LogLevel::Error);
  spdlogger_->error((tag_string + message).data(), args...);
}

template <class T, class... Rest>
void Logger::concatenate_messages(std::stringstream &ss, LogLevel log_level,
                                  size_t tag_length, bool first_message,
                                  T message, Rest... messages) const {
  if (!first_message) {
    pad_logging_stream_for_alignment(ss, log_level, tag_length);
  }
  ss << message;
  ss << std::endl;
  concatenate_messages(ss, log_level, tag_length, false, messages...);
}

template <class T>
void Logger::concatenate_messages(std::stringstream &ss, LogLevel log_level,
                                  size_t tag_length, bool first_message,
                                  T message) const {
  if (!first_message) {
    pad_logging_stream_for_alignment(ss, log_level, tag_length);
  }
  ss << message;
}

static void set_q_path_bench_script(int qid, std::__thread_id tid) {
  Logger::get()._update_q_path(qid, tid, Q_PATH_BENCH_SCRIPT_INDEX);
}

static void set_q_path_when_any(int qid, std::__thread_id tid) {
  Logger::get()._update_q_path(qid, tid, Q_PATH_TASK_OR_TIMER_INDEX);
  Logger::get()._update_q_path_tasks_completed_before_timer(qid, true);
}

static void set_q_path_timer_expire(int qid, std::__thread_id tid) {
  Logger::get()._update_q_path(qid, tid, Q_PATH_TASK_OR_TIMER_INDEX);
  Logger::get()._update_q_path_tasks_completed_before_timer(qid, false);
}

static void set_q_path_response_ready(int qid, std::__thread_id tid) {
  Logger::get()._update_q_path(qid, tid, Q_PATH_RESPONSE_READY_INDEX);
}

static void set_q_path_bench_cont(int qid, std::__thread_id tid) {
  Logger::get()._update_q_path(qid, tid, Q_PATH_BENCH_CONT_INDEX);
  Logger::get()._update_q_path_response_received(qid);
}

static std::unordered_map<int, std::pair<std::vector<std::__thread_id>, std::pair<int, bool>>> get_q_path_table() {
  return Logger::get().q_path_table;
}

static void update_bench_script_count(std::__thread_id tid) {
  Logger::get()._update_count(tid, BENCH_SCRIPT_INDEX);
}

static void update_when_any_count(std::__thread_id tid) {
  Logger::get()._update_count(tid, WHEN_ANY_INDEX);
}

static void update_response_ready_count(std::__thread_id tid) {
  Logger::get()._update_count(tid, RESPONSE_READY_INDEX);
}

static void update_bench_cont_count(std::__thread_id tid) {
  Logger::get()._update_count(tid, BENCH_CONT_INDEX);
}

static void update_timer_expire_count(std::__thread_id tid) {
  Logger::get()._update_count(tid, TIMER_EXPIRE_INDEX);
}

static std::unordered_map<std::__thread_id, std::vector<int>>
get_t_counts_table() {
  return Logger::get().t_counts_table;
};

template <class... Strings>
static void log_info(const std::string tag, Strings... messages) {
  Logger::get().log_info(tag, messages...);
}
template <class... Args>
static void log_info_formatted(const std::string tag, const char *message,
                               Args... args) {
  Logger::get().log_info_formatted(tag, message, args...);
}
template <class... Strings>
static void log_debug(const std::string tag, Strings... messages) {
  Logger::get().log_debug(tag, messages...);
}
template <class... Args>
static void log_debug_formatted(const std::string tag, const char *message,
                                Args... args) {
  Logger::get().log_debug_formatted(tag, message, args...);
}
template <class... Strings>
static void log_error(const std::string tag, Strings... messages) {
  Logger::get().log_error(tag, messages...);
}
template <class... Args>
static void log_error_formatted(const std::string tag, const char *message,
                                Args... args) {
  Logger::get().log_error_formatted(tag, message, args...);
}

}  // namespace clipper

#endif  // CLIPPER_LOGGING_HPP
