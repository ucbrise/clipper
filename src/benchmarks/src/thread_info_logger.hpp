namespace thread_info_logger {

class ThreadInfoLogger {

public:
  static constexpr int BENCH_SCRIPT_INDEX = 0;
  static constexpr int TASKS_COMPLETED_INDEX = 1;
  static constexpr int TIMER_EXPIRE_INDEX = 2;
  static constexpr int RESPONSE_READY_INDEX = 3;
  static constexpr int BENCH_CONT_INDEX = 4;

  /**
   * Obtains an instance of the Logger singleton
   * that can be used to log messages at specified levels
   */
  static ThreadInfoLogger &get() {
    static ThreadInfoLogger instance;
    return instance;
  }

  static void update_bench_script_count(std::__thread_id tid) {
    get().update_count(tid, ThreadInfoLogger::BENCH_SCRIPT_INDEX);
  }

  static void update_tasks_completed_count(std::__thread_id tid) {
    get().update_count(tid, ThreadInfoLogger::TASKS_COMPLETED_INDEX);
  }

  static void update_response_ready_count(std::__thread_id tid) {
    get().update_count(tid, ThreadInfoLogger::RESPONSE_READY_INDEX);
  }

  static void update_bench_cont_count(std::__thread_id tid) {
    get().update_count(tid, ThreadInfoLogger::BENCH_CONT_INDEX);
  }

  static void update_timer_expire_count(std::__thread_id tid) {
    get().update_count(tid, ThreadInfoLogger::TIMER_EXPIRE_INDEX);
  }

  static std::unordered_map <std::__thread_id, std::vector<int>> get_t_counts_table() {
    return get().t_counts_table;
  };

  static void create_q_path_entry(int qid) {
    get().add_q_path_entry(qid);
  }

  static void set_q_path_bench_script(int qid, std::__thread_id tid) {
    get().update_q_path(qid, tid, ThreadInfoLogger::BENCH_SCRIPT_INDEX);
  }

  static void set_q_path_tasks_completed(int qid, std::__thread_id tid) {
    get().update_q_path_task(qid, tid);
  }

  static void set_q_path_timer_expire(int qid, std::__thread_id tid) {
    get().update_q_path_timer(qid, tid);
  }

  static void set_q_path_response_ready(int qid, std::__thread_id tid) {
    get().update_q_path(qid, tid, ThreadInfoLogger::RESPONSE_READY_INDEX);
  }

  static void set_q_path_bench_cont(int qid, std::__thread_id tid) {
    get().update_q_path_response_received(qid, tid);
  }

//static std::unordered_map<int, std::pair<std::vector<std::__thread_id>, std::pair<std::atomic<int>, std::atomic<bool>>>> get_q_path_table() {
  static std::unordered_map<int, std::pair<std::vector<std::__thread_id>, std::pair<std::pair<double, double>, bool>>> get_q_path_table() {
    return ThreadInfoLogger::get().q_path_table;
  }

private:

  /**
   * Maps from TID ->
   *    vector(
   *        (int) count in benchmark script,
   *        (int) count in when_any,
   *        (int) count in timer expire,
   *        (int) count in response ready,
   *        (int) count in bench continuation
   *    )
   */
  std::unordered_map <std::__thread_id, std::vector<int>> t_counts_table;

  void update_count(std::__thread_id tid, int index) {
    if (t_counts_table.find(tid) == t_counts_table.end()) {
      t_counts_table[tid] = std::vector<int> {0, 0, 0, 0, 0};
    }

    auto vec = t_counts_table[tid];
    vec[index] += 1;
    t_counts_table[tid] = vec;
  }

//  std::unordered_map<int, std::pair<std::vector<std::__thread_id>, std::pair<std::atomic<int>, std::atomic<bool>>>> q_path_table;
  /**
   * Maps from Query ID ->
   *    pair(
   *        vector(
   *            (tid) id of thread that executed benchmark script,
   *            (tid) id for when_any,
   *            (tid) id for timer_expire,
   *            (tid) id for response_ready,
   *            (tid) id for bench continuation
   *        ),
   *        pair(
   *            pair(
   *                time (microseconds since epoch) at which tasks completed,
   *                time (microseconds since epoch) at which timer expired
   *            ),
   *            (bool) if the response for this query was received in the benchmark continuation
   *        )
   *   )
   */
  std::unordered_map<int, std::pair<std::vector<std::__thread_id>, std::pair<std::pair<double, double>, bool>>> q_path_table;

  void add_q_path_entry(int qid) {
    std::__thread_id tid = std::__thread_id();
    auto vec = std::vector<std::__thread_id>{tid, tid, tid, tid, tid};
    q_path_table.emplace(qid, std::make_pair(vec, std::make_pair(std::make_pair(0, 0), false)));
  }

  void update_q_path(int qid, std::__thread_id tid, int index) {
    q_path_table[qid].first[index] = tid;
  }

  void update_q_path_task(int qid, std::__thread_id tid) {
    update_q_path(qid, tid, ThreadInfoLogger::TASKS_COMPLETED_INDEX);
    q_path_table[qid].second.first.first = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  }

  void update_q_path_timer(int qid, std::__thread_id tid) {
    update_q_path(qid, tid, ThreadInfoLogger::TIMER_EXPIRE_INDEX);
    q_path_table[qid].second.first.second = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  }

  void update_q_path_response_received(int qid, std::__thread_id tid) {
    update_q_path(qid, tid, ThreadInfoLogger::BENCH_CONT_INDEX);
    q_path_table[qid].second.second = true;
  }
};


} // namespace thread_info_logger
