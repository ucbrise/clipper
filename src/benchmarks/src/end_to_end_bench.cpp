#include <fstream>
#include <functional>
#include <iostream>
#include <random>
#include <vector>

#include <boost/thread.hpp>
#include <cxxopts.hpp>

#include <clipper/constants.hpp>
#include <clipper/datatypes.hpp>
#include <clipper/future.hpp>
#include <clipper/json_util.hpp>
#include <clipper/logging.hpp>
#include <clipper/query_processor.hpp>

#include "bench_utils.hpp"

const std::string CIFAR_DATA_PATH = "cifar_data_path";
const std::string NUM_THREADS = "num_threads";
const std::string NUM_BATCHES = "num_batches";
const std::string REQUEST_BATCH_SIZE = "request_batch_size";
const std::string REQUEST_BATCH_DELAY_MICROS = "request_batch_delay_micros";
const std::string LATENCY_OBJECTIVE = "latency_objective";
const std::string REPORT_DELAY_SECONDS = "report_delay_seconds";
const std::string BENCHMARK_REPORT_PATH = "benchmark_report_path";
const std::string POISSON_DELAY = "poisson_delay";
const std::string MODEL_NAME = "model_name";
const std::string MODEL_VERSION = "model_version";
const std::string PREVENT_CACHE_HITS = "prevent_cache_hits";
const std::string THREAD_COUNTS_REPORT_PATH = "thread_counts_report_path";
const std::string SLEEP_AFTER_SEND_TIME_SEC = "sleep_afer_send_time_sec";

const int FLUSH_AT_END_INDICATOR = -1;

const std::string DEFAULT_OUTPUT = "-1";
const std::string TEST_APPLICATION_LABEL = "cifar_bench";
const int UID = 0;

std::atomic<bool> all_requests_sent(false);

using namespace clipper;
using namespace bench_utils;

std::string get_window_str(int window_lower, int window_upper) {
  std::stringstream ss;
  ss << std::to_string(window_lower) << "s - " << std::to_string(window_upper)
     << "s";
  return ss.str();
}

void send_predictions(std::unordered_map<std::string, std::string> &config,
                      QueryProcessor &qp, std::vector<std::vector<double>> data,
                      BenchMetrics &bench_metrics, int thread_id) {
  int num_batches = get_int(NUM_BATCHES, config);
  int request_batch_size = get_int(REQUEST_BATCH_SIZE, config);
  long batch_delay_micros = get_long(REQUEST_BATCH_DELAY_MICROS, config);
  int latency_objective = get_int(LATENCY_OBJECTIVE, config);
  std::string model_name = get_str(MODEL_NAME, config);
  std::string model_version = get_str(MODEL_VERSION, config);
  bool draw_from_poisson = get_bool(POISSON_DELAY, config);
  bool prevent_cache_hits = get_bool(PREVENT_CACHE_HITS, config);

  int num_datapoints = static_cast<int>(data.size());
  std::vector<double> query_vec;
  std::default_random_engine generator;
  std::poisson_distribution<int> distribution(batch_delay_micros);
  long delay_micros;
  int query_num;

  for (int j = 0; j < num_batches; j++) {
    for (int i = 0; i < request_batch_size; i++) {
      query_num = j * request_batch_size + i;
      query_vec = data[query_num % num_datapoints];

      if (prevent_cache_hits) {
        // Modify it to be epoch and thread-specific
        query_vec[0] = query_num;
        query_vec[1] += thread_id;
      }

      std::shared_ptr<Input> input = std::make_shared<DoubleVector>(query_vec);
      Query q = {TEST_APPLICATION_LABEL,
                 UID,
                 input,
                 latency_objective,
                 clipper::DefaultOutputSelectionPolicy::get_name(),
                 {VersionedModelId(model_name, model_version)},
                 query_num};
      bench_metrics.request_throughput_->mark(1);

      set_q_path_bench_script(query_num, std::this_thread::get_id());
      update_bench_script_count(std::this_thread::get_id());
      log_info("qid", query_num, "send request", std::this_thread::get_id());

      if (SEND_REQUESTS) {
        boost::future<Response> prediction = qp.predict(q);
        prediction.then([bench_metrics](boost::future<Response> f) {
          Response r = f.get();

          set_q_path_bench_cont(r.query_.test_qid_,
                                              std::this_thread::get_id());
          update_bench_cont_count(std::this_thread::get_id());
          log_info("qid", r.query_.test_qid_, "bench continuation", std::this_thread::get_id());

          // Update metrics
          if (r.output_is_default_) {
            bench_metrics.default_pred_ratio_->increment(1, 1);
          } else {
            bench_metrics.default_pred_ratio_->increment(0, 1);
          }
          bench_metrics.latency_->insert(r.duration_micros_);
          bench_metrics.num_predictions_->increment(1);
          bench_metrics.throughput_->mark(1);
        });
      }
    }

    delay_micros =
        draw_from_poisson ? distribution(generator) : batch_delay_micros;
    if (delay_micros > 0) {
      std::this_thread::sleep_for(std::chrono::microseconds(delay_micros));
    }
  }
}

void document_benchmark_details(
    std::unordered_map<std::string, std::string> &config,
    std::ofstream &report_file) {
  std::stringstream ss;
  ss << "---Configuration---" << std::endl;
  for (auto it : config) ss << it.first << ": " << it.second << std::endl;
  ss << "-------------------" << std::endl;
  std::string configuration_details = ss.str();
  report_file << configuration_details;
  log_info("CONFIG", configuration_details);
}

void report_window_details(std::ofstream &report_file, std::string window) {
  auto metrics = metrics::MetricsRegistry::get_metrics().report_metrics(true);
  report_file << window << ": " << metrics;
  report_file.flush();

  log_info("WINDOW", window);
  log_info("METRICS", metrics);
}

void repeatedly_report_and_clear_metrics(
    std::unordered_map<std::string, std::string> &config) {
  int report_delay_seconds = get_int(REPORT_DELAY_SECONDS, config);
  std::string latency_obj_string = get_str(LATENCY_OBJECTIVE, config);
  std::string batch_delay_string = get_str(REQUEST_BATCH_DELAY_MICROS, config);
  std::string report_path = get_str(BENCHMARK_REPORT_PATH, config);

  // Write out run details
  std::ofstream report_file(report_path);
  document_benchmark_details(config, report_file);

  std::string window, metrics;
  int window_num = 1;
  int window_lower, window_upper;

  while (!all_requests_sent) {
    // Wait for metrics
    std::this_thread::sleep_for(std::chrono::seconds(report_delay_seconds));

    window_lower = report_delay_seconds * (window_num - 1);
    window_upper = report_delay_seconds * window_num;
    window = get_window_str(window_lower, window_upper);
    report_window_details(report_file, window);

    window_num += 1;
  }
}

void write_full_report(std::unordered_map<std::string, std::string> &config,
                       double start_time_seconds) {
  std::string report_path = get_str(BENCHMARK_REPORT_PATH, config);

  double end_time_seconds =
      std::chrono::duration_cast<std::chrono::seconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count();
  int window_len_seconds = (int)(end_time_seconds - start_time_seconds);

  std::string window = get_window_str(0, window_len_seconds);

  // Write out run details
  std::ofstream report_file(report_path);
  document_benchmark_details(config, report_file);
  report_window_details(report_file, window);
}

struct Hasher {
  std::size_t operator()(const std::pair<int, int>& k) const {
    using std::size_t;
    using std::hash;
    using std::string;

    std::size_t seed = 0;
    boost::hash_combine(seed, k.first);
    boost::hash_combine(seed, k.second);
    return seed;
  }
};

void update_sim_table(std::unordered_map<std::pair<int, int>, int, Hasher>& sim_table, std::vector<std::__thread_id>& vec) {
  for (int i = 0; i < static_cast<int>(vec.size()); i++) {
    for (int j = 0; j < static_cast<int>(vec.size()); j++) {
      if (vec[i] == vec[j]) {
        auto key = std::make_pair(i, j);
        if (sim_table.find(key) == sim_table.end()) {
          sim_table[key] = 0;
        }
        sim_table[key] += 1;
      }
    }
  };
}

void report_t_counts_metrics(
    std::unordered_map<std::string, std::string> config) {
  std::string report_path = get_str(THREAD_COUNTS_REPORT_PATH, config);
  std::ofstream report_file(report_path);
  document_benchmark_details(config, report_file);

  log_info("main", "report_t_counts_metrics tid:", std::this_thread::get_id());

  std::stringstream ss_logging_consts;
  ss_logging_consts << "SHORT_CIRCUIT_TASK_EXECUTOR: " << SHORT_CIRCUIT_TASK_EXECUTOR << ", ";
  ss_logging_consts << "SEND_REQUESTS: " << SEND_REQUESTS << ", ";
  ss_logging_consts << "IGNORE_OVERDUE_TASKS: " << IGNORE_OVERDUE_TASKS << ", ";
  ss_logging_consts << "USE_FIXED_BATCH_SIZE: " << USE_FIXED_BATCH_SIZE << ", ";
  ss_logging_consts << "FIXED_BATCH_SIZE: " << FIXED_BATCH_SIZE << std::endl;
  std::string logging_constants_info = ss_logging_consts.str();
  log_info("CONSTS", logging_constants_info);
  report_file << logging_constants_info << "--------------" << std::endl;

  std::stringstream ss_indices;
  ss_indices << "BENCH_SCRIPT_INDEX: " << BENCH_SCRIPT_INDEX << ", ";
  ss_indices << "WHEN_ANY_INDEX: " << WHEN_ANY_INDEX << ", ";
  ss_indices << "TIMER_EXPIRE_INDEX: " << TIMER_EXPIRE_INDEX << ", ";
  ss_indices << "RESPONSE_READY_INDEX: " << RESPONSE_READY_INDEX << ", ";
  ss_indices << "BENCH_CONT_INDEX: " << BENCH_CONT_INDEX << std::endl;
  std::string indices_info = ss_indices.str();
  log_info("INDICES", indices_info);
  report_file << indices_info << std::endl;

  auto t_counts_table = get_t_counts_table();
  std::stringstream t_ss;
  std::vector<int> num_threads_executing_event = {0, 0, 0, 0, 0};
  t_ss << std::endl;
  for (auto it = begin(t_counts_table); it != end(t_counts_table); ++it) {
    t_ss << it->first << ": ";
    int i = 0;
    for (auto el : it->second) {
      t_ss << el << ", ";
      if (el > 0) {
        num_threads_executing_event[i] += 1;
      }
      i += 1;
    }
    t_ss << std::endl;
  }

  std::stringstream num_threads_executing_event_ss;
  for (auto el : num_threads_executing_event) {
    num_threads_executing_event_ss << el << " : ";
  }
  report_file << "num_threads_executing_event_ss:    " << num_threads_executing_event_ss.str() << std::endl;


  std::string table = t_ss.str();
  log_info("TABLE", table);
  report_file << table << "--------------" << std::endl;

  std::unordered_map<std::pair<int, int>, int, Hasher> task_completed_same_thread;
  std::unordered_map<std::pair<int, int>, int, Hasher> timer_completed_same_thread;
  int task_completed_count = 0;
  int neither_completed_count = 0;
  int timer_completed_count = 0;
  int num_incomplete = 0;
  auto q_path_table = get_q_path_table();
  std::stringstream q_ss;
  for  (auto it = begin(q_path_table); it != end(q_path_table); ++it) {
    std::vector<std::__thread_id> vec = it->second.first;
    int who_completed  = it->second.second.first;
    bool response_received  = it->second.second.second;
    if (!response_received) {
      num_incomplete += 1;
      continue;
    }
    if (who_completed == COMPLETED_BY_TASK) {
      task_completed_count += 1;
      update_sim_table(task_completed_same_thread, vec);
    } else if (who_completed == COMPLETED_BY_TIMER) {
      timer_completed_count += 1;
      update_sim_table(timer_completed_same_thread, vec);
    } else if (who_completed == COMPLETED_BY_NEITHER) {
      neither_completed_count += 1;
    }
  }

  std::stringstream ss_task;
  std::stringstream ss_time;
  for (int i = Q_PATH_BENCH_SCRIPT_INDEX; i <= Q_PATH_BENCH_CONT_INDEX; i++) {
    for (int j = Q_PATH_BENCH_SCRIPT_INDEX; j <= Q_PATH_BENCH_CONT_INDEX; j++) {
      std::pair<int, int> key = std::make_pair(i, j);
      ss_task << task_completed_same_thread[key] << ", ";
      ss_time << timer_completed_same_thread[key] << ", ";
     }
    ss_task << std::endl;
    ss_time << std::endl;
  };
  std::string task_string = ss_task.str();
  std::string timer_string = ss_time.str();

  std::stringstream q_p_constants;
  q_p_constants << "event " << SHORT_CIRCUIT_TASK_EXECUTOR << ": bench script sends request" << std::endl;
  q_p_constants << "event " << Q_PATH_TASK_OR_TIMER_INDEX << ": response_ready_future gets completed" << std::endl;
  q_p_constants << "event " << Q_PATH_RESPONSE_READY_INDEX << ": response_ready_future continuation gets run " << std::endl;
  q_p_constants << "event " << Q_PATH_BENCH_CONT_INDEX << ": bench script continuation gets run" << std::endl;
  std::string q_p_info = q_p_constants.str();
  log_info("QP_INFO", q_p_info);
  report_file << std::endl;
  report_file << q_p_info;
  report_file << std::endl;

  std::stringstream similarity_ss;

  similarity_ss << "Number of queries whose response_ready_future was completed by all_tasks_completed: " << task_completed_count << std::endl;
  similarity_ss << "Number completed by timer_future: " << timer_completed_count << std::endl;
  similarity_ss << "Number completed by neither (should happen when models aren't connected): " << neither_completed_count << std::endl;
  similarity_ss << "Number of queries that didn't receive a response: " << num_incomplete << std::endl;
  similarity_ss << "Matrix[i, j] corresponds to event at the number of queries for which the thread that executed event i also executed thread j" << std::endl << std::endl;
  similarity_ss << "response_ready_future future completed by all_tasks_completed: " <<  std::endl << task_string << std::endl;
  similarity_ss << "response_ready_future future completed by timer_future: " <<  std::endl << timer_string;
  std::string similarity_info = similarity_ss.str();

  log_info("QPATH", similarity_info);
  report_file << similarity_info;
  report_file.flush();
}



void run_benchmark(std::unordered_map<std::string, std::string> &config) {
  get_config().ready();
  QueryProcessor qp;
  std::this_thread::sleep_for(std::chrono::seconds(3));

  clipper::DefaultOutputSelectionPolicy p;
  clipper::Output parsed_default_output(DEFAULT_OUTPUT, {});
  auto init_state = p.init_state(parsed_default_output);
  clipper::StateKey state_key{TEST_APPLICATION_LABEL, clipper::DEFAULT_USER_ID,
                              0};
  qp.get_state_table()->put(state_key, p.serialize(init_state));

  // We only need the datapoints – not their labels.
  std::string cifar_data_path = get_str(CIFAR_DATA_PATH, config);
  auto cifar_data = load_cifar(cifar_data_path);
  std::vector<std::vector<double>> datapoints =
      concatenate_cifar_datapoints(cifar_data);
  int num_threads = get_int(NUM_THREADS, config);

  BenchMetrics bench_metrics(TEST_APPLICATION_LABEL);
  std::vector<std::thread> threads;

  double start_time_seconds =
      std::chrono::duration_cast<std::chrono::seconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count();

  for (int j = 0; j < num_threads; j++) {
//    std::vector<std::vector<double>> thread_datapoints(datapoints);
    std::thread thread([&config, &qp, datapoints, &bench_metrics, j]() {
      send_predictions(config, qp, datapoints, bench_metrics, j);
    });
    threads.push_back(std::move(thread));
  }

  int report_delay_seconds = get_int(REPORT_DELAY_SECONDS, config);
  bool flush_at_end = report_delay_seconds == FLUSH_AT_END_INDICATOR;

  std::thread metrics_thread;

  if (!flush_at_end) {
    metrics_thread =
        std::thread([&]() { repeatedly_report_and_clear_metrics(config); });
  }

  for (auto &thread : threads) {
    thread.join();
  }
  all_requests_sent = true;

  if (flush_at_end) {
    write_full_report(config, start_time_seconds);
  } else {
    metrics_thread.join();
  }

  log_info("main", "run_bench tid:", std::this_thread::get_id());

  int sleep_amt = get_int(SLEEP_AFTER_SEND_TIME_SEC, config);
  if (sleep_amt > 0) {
    std::this_thread::sleep_for(std::chrono::seconds(sleep_amt));
  }

  std::thread report_t_counts_metrics_thread(
      [&config]() { report_t_counts_metrics(config); });
  report_t_counts_metrics_thread.join();

  log_info("BENCH", "Terminating benchmarking script");
  std::terminate();
}

int main(int argc, char *argv[]) {
  cxxopts::Options options("generic_bench",
                           "Clipper generic performance benchmarking");
  // clang-format off
  options.add_options()
          ("f,filename", "Config file name", cxxopts::value<std::string>());
  // clang-format on
  options.parse(argc, argv);
  bool json_specified = (options.count("filename") > 0);

  std::vector<std::string> desired_vars = {
      CIFAR_DATA_PATH,    NUM_BATCHES,          REQUEST_BATCH_DELAY_MICROS,
      LATENCY_OBJECTIVE,  REPORT_DELAY_SECONDS, BENCHMARK_REPORT_PATH,
      POISSON_DELAY,      MODEL_NAME,           MODEL_VERSION,
      REQUEST_BATCH_SIZE, NUM_THREADS,          PREVENT_CACHE_HITS,
      THREAD_COUNTS_REPORT_PATH, SLEEP_AFTER_SEND_TIME_SEC};
  if (!json_specified) {
    throw std::invalid_argument("No configuration file provided");
  } else {
    std::string json_path = options["filename"].as<std::string>();
    std::unordered_map<std::string, std::string> config =
        get_config_from_json(json_path, desired_vars);
    run_benchmark(config);
  }
}
