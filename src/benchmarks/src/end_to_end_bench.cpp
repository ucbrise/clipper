#include <fstream>
#include <functional>
#include <iostream>
#include <random>
#include <vector>

#include <folly/futures/Future.h>
#include <cxxopts.hpp>

#include <clipper/constants.hpp>
#include <clipper/datatypes.hpp>
#include <clipper/json_util.hpp>
#include <clipper/logging.hpp>
#include <clipper/memory.hpp>
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
  std::default_random_engine generator;
  std::poisson_distribution<int> distribution(batch_delay_micros);
  long delay_micros;
  int query_num;

  for (int j = 0; j < num_batches; j++) {
    for (int i = 0; i < request_batch_size; i++) {
      query_num = j * request_batch_size + i;
      auto &query_vec = data[query_num % num_datapoints];
      UniquePoolPtr<double> query_data =
          memory::allocate_unique<double>(query_vec.size());
      memcpy(query_data.get(), query_vec.data(),
             query_vec.size() * sizeof(double));
      double *query_data_raw = query_data.get();
      if (prevent_cache_hits) {
        // Modify it to be epoch and thread-specific
        query_data_raw[0] = query_num / num_datapoints;
        query_data_raw[1] = thread_id;
      }

      std::shared_ptr<PredictionData> input = std::make_shared<DoubleVector>(
          std::move(query_data), query_vec.size());
      Query q = {TEST_APPLICATION_LABEL,
                 UID,
                 input,
                 latency_objective,
                 clipper::DefaultOutputSelectionPolicy::get_name(),
                 {VersionedModelId(model_name, model_version)}};

      folly::Future<Response> prediction = qp.predict(q);
      bench_metrics.request_throughput_->mark(1);

      std::move(prediction).thenValue([bench_metrics](Response r) {
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
    std::thread thread([&config, &qp, datapoints, &bench_metrics, j]() {
      send_predictions(config, qp, datapoints, bench_metrics, j);
    });
    threads.push_back(std::move(thread));
  }

  int report_delay_seconds = get_int(REPORT_DELAY_SECONDS, config);
  bool flush_at_end = report_delay_seconds == FLUSH_AT_END_INDICATOR;

  std::thread metrics_thread;

  if (!flush_at_end) {
    metrics_thread = std::thread(
        [&config]() { repeatedly_report_and_clear_metrics(config); });
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

  log_info("BENCH", "Terminating benchmarking script");
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
      REQUEST_BATCH_SIZE, NUM_THREADS,          PREVENT_CACHE_HITS};
  if (!json_specified) {
    throw std::invalid_argument("No configuration file provided");
  } else {
    std::string json_path = options["filename"].as<std::string>();
    std::unordered_map<std::string, std::string> config =
        get_config_from_json(json_path, desired_vars);
    run_benchmark(config);
  }
}
