#include <functional>
#include <iostream>
#include <vector>

#include <boost/thread.hpp>
#include <cxxopts.hpp>

#include <clipper/constants.hpp>
#include <clipper/datatypes.hpp>
#include <clipper/future.hpp>
#include <clipper/json_util.hpp>
#include <clipper/logging.hpp>
#include <clipper/query_processor.hpp>
#include <fstream>
#include <random>

#include "include/bench_utils.hpp"

const std::string CIFAR_DATA_PATH = "cifar_data_path";
const std::string NUM_THREADS = "num_threads";
const std::string NUM_BATCHES = "num_batches";
const std::string REQUEST_BATCH_SIZE = "request_batch_size";
const std::string REQUEST_BATCH_DELAY_MICROS = "request_batch_delay_micros";
const std::string LATENCY_OBJECTIVE = "latency_objective";
const std::string REPORT_DELAY_SECONDS = "report_delay_seconds";
const std::string REPORTS_PATH = "reports_path";
const std::string POISSON_DELAY = "poisson_delay";
const std::string MODEL_NAME = "model_name";
const std::string MODEL_VERSION = "model_version";
const std::string PREVENT_CACHE_HITS = "prevent_cache_hits";

const std::string DEFAULT_OUTPUT = "-1";
const std::string TEST_APPLICATION_LABEL = "cifar_bench";
const int UID = 0;

using namespace clipper;
using namespace bench_utils;

std::string _get_window_str(int window_size, int num_iters) {
  int window_lower = window_size * (num_iters - 1);
  int window_upper = window_size * num_iters;
  std::stringstream ss;
  ss << std::to_string(window_lower) << "s - " << std::to_string(window_upper)
     << "s";
  return ss.str();
}

void send_predictions(std::unordered_map<std::string, std::string> &config,
                      QueryProcessor &qp, std::vector<std::vector<double>> data,
                      std::vector<double> labels, BenchMetrics &bench_metrics,
                      int thread_id, std::vector<int> indices) {
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
  int data_index;
  double label;
  std::vector<double> input_vector_data;

  for (int j = 0; j < num_batches; j++) {
    std::vector<boost::future<Response>> futures;
    std::vector<double> future_labels;
    for (int i = 0; i < request_batch_size; i++) {
      // Select datapoint
      data_index = indices[(j * request_batch_size + i) % num_datapoints];
      query_vec = data[data_index];
      label = labels[data_index];

      if (prevent_cache_hits) {
        // Modify it to be epoch and thread-specific
        query_vec[0] = (j * request_batch_size + i) / num_datapoints;
        query_vec[1] = thread_id;
      }

      // Copy the datapoint into a new shared pointer
      std::shared_ptr<Input> input = std::make_shared<DoubleVector>(query_vec);

      boost::future<Response> prediction =
          qp.predict({TEST_APPLICATION_LABEL,
                      UID,
                      input,
                      latency_objective,
                      clipper::DefaultOutputSelectionPolicy::get_name(),
                      {VersionedModelId(model_name, model_version)}});

      futures.push_back(std::move(prediction));
      future_labels.push_back(std::move(label));
      bench_metrics.send_rate_->mark(1);
    }

    std::shared_ptr<std::atomic_int> completed =
        std::make_shared<std::atomic_int>(0);
    std::pair<boost::future<void>, std::vector<boost::future<Response>>>
        results = future::when_all(std::move(futures), completed);
    results.first.get();
    for (int i = 0; i < static_cast<int>(results.second.size()); i++) {
      boost::future<Response> &f = results.second[i];
      Response r = f.get();

      // Update metrics
      if (r.output_is_default_) {
        bench_metrics.default_pred_ratio_->increment(1, 1);
      } else {
        bench_metrics.default_pred_ratio_->increment(0, 1);
      }
      bench_metrics.latency_->insert(r.duration_micros_);
      bench_metrics.num_predictions_->increment(1);
      bench_metrics.throughput_->mark(1);

      if (std::stod(r.output_.y_hat_) == future_labels[i]) {
        bench_metrics.accuracy_ratio_->increment(1, 1);
      } else {
        bench_metrics.accuracy_ratio_->increment(0, 1);
      }
    }

    delay_micros =
        draw_from_poisson ? distribution(generator) : batch_delay_micros;
    std::this_thread::sleep_for(std::chrono::microseconds(delay_micros));
  }
}

void report_and_clear_metrics(
    std::unordered_map<std::string, std::string> &config) {
  int report_delay_seconds = get_int(REPORT_DELAY_SECONDS, config);
  std::string latency_obj_string = get_str(LATENCY_OBJECTIVE, config);
  std::string batch_delay_string = get_str(REQUEST_BATCH_DELAY_MICROS, config);
  std::string reports_path = get_str(REPORTS_PATH, config);

  // Write out run details
  std::ofstream reports(reports_path);
  std::stringstream ss;
  ss << "---Configuration---" << std::endl;
  for (auto it : config) ss << it.first << ": " << it.second << std::endl;
  ss << "-------------------" << std::endl;
  std::string configuration_details = ss.str();
  reports << configuration_details;
  log_info("BENCH", configuration_details);

  std::string window, metrics;
  int window_num = 1;

  // This will get interrupted
  while (true) {
    // Wait for metrics
    std::this_thread::sleep_for(std::chrono::seconds(report_delay_seconds));

    metrics = metrics::MetricsRegistry::get_metrics().report_metrics(true);
    window = _get_window_str(report_delay_seconds, window_num);

    reports << window << ": " << metrics;
    reports.flush();

    log_info("WINDOW", window);
    log_info("METRICS", metrics);
    window_num += 1;
  }
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
  std::unordered_map<int, std::vector<std::vector<double>>> cifar_data =
      load_cifar(cifar_data_path);
  auto concatenated_datapoints = concatenate_cifar_datapoints(cifar_data);
  auto datapoints = concatenated_datapoints.first;
  auto labels = concatenated_datapoints.second;

  int num_threads = get_int(NUM_THREADS, config);
  size_t num_datapoints = datapoints.size();
  unsigned int seed = 0;
  std::vector<std::vector<int>> indices_for_threads(
      static_cast<size_t>(num_threads));

  // Assign shuffled datapoint selection indices for each thread
  for (int j = 0; j < num_threads; j++) {
    std::vector<int> indices(num_datapoints);
    for (int i = 0; i < static_cast<int>(num_datapoints); i++) {
      indices[i] = i;
    }
    shuffle(indices.begin(), indices.end(), std::default_random_engine(seed));
    indices_for_threads.push_back(std::move(indices));
  }

  BenchMetrics bench_metrics(TEST_APPLICATION_LABEL);
  std::vector<std::thread> threads;
  for (int j = 0; j < num_threads; j++) {
    std::thread thread([&]() {
      send_predictions(config, qp, datapoints, labels, bench_metrics, j,
                       indices_for_threads[j]);
    });
    threads.push_back(std::move(thread));
  }

  std::thread metrics_thread([&]() { report_and_clear_metrics(config); });

  for (auto &thread : threads) {
    thread.join();
  }

  log_info("BENCH", "Terminating benchmarking script");

  // Kills the metrics thread. We don't care about the last report anyway.
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
      LATENCY_OBJECTIVE,  REPORT_DELAY_SECONDS, REPORTS_PATH,
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
