#include <time.h>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <boost/thread.hpp>
#include <cxxopts.hpp>

#include <rapidjson/document.h>
#include <clipper/constants.hpp>
#include <clipper/datatypes.hpp>
#include <clipper/future.hpp>
#include <clipper/json_util.hpp>
#include <clipper/logging.hpp>
#include <clipper/query_processor.hpp>
#include <fstream>

using namespace clipper;

const std::string DEFAULT_OUTPUT = "-1";
const std::string TEST_APPLICATION_LABEL = "test";
const std::string NOOP_MODEL_NAME = "bench_noop";

const int RANDOM_VEC_LEN = 784;
const int RANDOM_VEC_UPPERBOUND = 10;

const std::string CONFIG_KEY_NUM_THREADS = "num_threads";
const std::string CONFIG_KEY_NUM_BATCHES = "num_batches";
const std::string CONFIG_KEY_BATCH_SIZE = "batch_size";
const std::string CONFIG_KEY_BATCH_DELAY = "batch_delay";


class AppMetrics {
  public:
    explicit AppMetrics(std::string app_name)
        : app_name_(app_name),
          latency_(
                  clipper::metrics::MetricsRegistry::get_metrics().create_histogram(
                          "app:" + app_name + ":prediction_latency", "microseconds",
                          4096)),
          throughput_(
                  clipper::metrics::MetricsRegistry::get_metrics().create_meter(
                          "app:" + app_name + ":prediction_throughput")),
          num_predictions_(
                  clipper::metrics::MetricsRegistry::get_metrics().create_counter(
                          "app:" + app_name + ":num_predictions")),
          default_pred_ratio_(
                  clipper::metrics::MetricsRegistry::get_metrics()
                          .create_ratio_counter("app:" + app_name +
                                                ":default_prediction_ratio")) {}
      ~AppMetrics() = default;
    AppMetrics(const AppMetrics&) = default;
    AppMetrics& operator=(const AppMetrics&) = default;

    AppMetrics(AppMetrics&&) = default;
    AppMetrics& operator=(AppMetrics&&) = default;

    std::string app_name_;
    std::shared_ptr<clipper::metrics::Histogram> latency_;
    std::shared_ptr<clipper::metrics::Meter> throughput_;
    std::shared_ptr<clipper::metrics::Counter> num_predictions_;
    std::shared_ptr<clipper::metrics::RatioCounter> default_pred_ratio_;
};

std::vector<double> gen_random_vector(int length, int upper_bound) {
  std::vector<double> vec(length);
  for (size_t i = 0; i< vec.size(); i++) {
    vec[i] = std::rand() % upper_bound;
  }
  return vec;
}


void send_predictions(
    std::unordered_map<std::string, std::string> &config,
    QueryProcessor &qp) {

  int num_batches = std::stoi(config.find(CONFIG_KEY_NUM_BATCHES)->second);
  int batch_size = std::stoi(config.find(CONFIG_KEY_BATCH_SIZE)->second);
  long batch_delay_millis =
          static_cast<long>(std::stoi(config.find(CONFIG_KEY_BATCH_DELAY)->second));

  AppMetrics app_metrics(TEST_APPLICATION_LABEL);
  std::shared_ptr<metrics::Histogram> msg_latency_hist_;

  // get query vec
  std::vector<double> query_vec;

  for (int j = 0; j < num_batches; j++) {
    // batch sizes of 1 for now
    query_vec = gen_random_vector(RANDOM_VEC_LEN, RANDOM_VEC_UPPERBOUND);

    std::shared_ptr<Input> input = std::make_shared<DoubleVector>(query_vec);
    boost::future<Response> prediction =
        qp.predict({TEST_APPLICATION_LABEL,
                    0,
                    input,
                    100000,
                    clipper::DefaultOutputSelectionPolicy::get_name(),
                    {std::make_pair(NOOP_MODEL_NAME, "1")}});

    prediction.then([app_metrics](boost::future<Response> f) {
        Response r = f.get();

        // Update metrics
        if (r.output_is_default_) {
          app_metrics.default_pred_ratio_->increment(1, 1);
        } else {
          app_metrics.default_pred_ratio_->increment(0, 1);
        }
        app_metrics.latency_->insert(r.duration_micros_);
        app_metrics.num_predictions_->increment(1);
        app_metrics.throughput_->mark(1);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(batch_delay_millis));
  }
}

std::unordered_map<std::string, std::string> create_config(
        std::string num_threads, std::string num_batches,
        std::string batch_size, std::string batch_delay) {
  std::unordered_map<std::string, std::string> config;
  config.emplace(CONFIG_KEY_NUM_THREADS, num_threads);
  config.emplace(CONFIG_KEY_NUM_BATCHES, num_batches);
  config.emplace(CONFIG_KEY_BATCH_SIZE, batch_size);
  config.emplace(CONFIG_KEY_BATCH_DELAY, batch_delay);
  return config;
};

std::unordered_map<std::string, std::string> get_config_from_prompt() {
  std::string num_threads;
  std::string num_batches;
  std::string batch_size;
  std::string batch_delay;

  std::cout << "Before proceeding, run bench/noop_bench_setup.sh from clipper's "
          "root directory."
            << std::endl;
  std::cout << "Enter the number of threads of execution: ";
  std::cin >> num_threads;
  std::cout
          << "Enter the number of request batches to be sent by each thread: ";
  std::cin >> num_batches;
  std::cout << "Enter the number of requests per batch: ";
  std::cin >> batch_size;
  std::cout << "Enter the delay between batches, in milliseconds: ";
  std::cin >> batch_delay;

  return create_config(num_threads, num_batches, batch_size, batch_delay);
};

int main(int argc, char *argv[]) {
  cxxopts::Options options("noop_bench",
                           "Clipper noop performance benchmarking");

  std::unordered_map<std::string, std::string> test_config = get_config_from_prompt();

  get_config().ready();
  QueryProcessor qp;
  std::this_thread::sleep_for(std::chrono::seconds(3));

  clipper::DefaultOutputSelectionPolicy p;
  clipper::Output parsed_default_output(DEFAULT_OUTPUT, {});
  auto init_state = p.init_state(parsed_default_output);
  clipper::StateKey state_key{TEST_APPLICATION_LABEL, clipper::DEFAULT_USER_ID,
                              0};
  qp.get_state_table()->put(state_key, p.serialize(init_state));

  // Seed the random number generator that will be used to randomly generate datapoints
  std::srand(time(NULL));
  int num_threads = std::stoi(test_config.find(CONFIG_KEY_NUM_THREADS)->second);
  std::vector<std::thread> threads;
  for (int i = 0; i < num_threads; i++) {
    std::thread thread([&]() {
        send_predictions(test_config, qp);
    });
    threads.push_back(std::move(thread));
  }
  for (auto &thread : threads) {
    thread.join();
  }

  std::string metrics =
      metrics::MetricsRegistry::get_metrics().report_metrics();
  log_info("BENCH", metrics);
}
