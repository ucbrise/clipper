#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <functional>
#include <math.h>
#include <boost/thread.hpp>

#include <clipper/constants.hpp>
#include <clipper/datatypes.hpp>
#include <clipper/logging.hpp>
#include <clipper/query_processor.hpp>
#include <clipper/future.hpp>
#include <clipper/config.hpp>
#include <fstream>

using namespace clipper;

const std::string SKLEARN_MODEL_NAME = "bench_sklearn_cifar";
constexpr long NUM_THREADS = 10;
constexpr long NUM_BATCHES = 10;
constexpr long NUM_REQUESTS_PER_BATCH = 100;
constexpr long SLEEP_PER_BATCH_SECONDS = 1;

struct AddSquared : public std::binary_function<double, double, double> {
  double operator() (double a, double b) {
    return a + (b * b);
  }
};

void normalize_cifar(std::vector<std::vector<double>>& cifar_data) {
  std::vector<double> mean_vec(3072, 0);
  for(std::vector<double>& vec : cifar_data) {
    std::transform(mean_vec.begin(), mean_vec.end(), vec.begin(), mean_vec.begin(), std::plus<double>());
  }
  double data_length = static_cast<double>(cifar_data.size());
  for(int i = 0; i < mean_vec.size(); i++) {
    mean_vec[i] = mean_vec[i] / data_length;
  }
  for(std::vector<double>& vec : cifar_data) {
    std::transform(vec.begin(), vec.end(), mean_vec.begin(), vec.begin(), std::minus<double>());
  }
  std::vector<double> std_dev_vec(3072, 0);
  for(std::vector<double>& vec : cifar_data) {
    std::transform(std_dev_vec.begin(), std_dev_vec.end(), vec.begin(), std_dev_vec.begin(), AddSquared());
  }
  for(int i = 0; i < std_dev_vec.size(); i++) {
    std_dev_vec[i] = std::sqrt(std_dev_vec[i] / data_length);
  }
  for(std::vector<double>& vec : cifar_data) {
    std::transform(vec.begin(), vec.end(), std_dev_vec.begin(), vec.begin(), std::divides<double>());
  }
}

std::vector<std::pair<int, std::vector<double>>> load_cifar() {
  std::ifstream cifar_file("../../../bench/data/cifar-10-batches-bin/data_batch_1.bin", std::ios::binary);
  std::istreambuf_iterator<char> cifar_data(cifar_file);
  std::vector<std::vector<double>> cifar_vecs;
  std::vector<int> cifar_labels;
  for (int i = 0; i < 10000; i++) {
    int label = static_cast<int>(*cifar_data);
    cifar_data++;
    std::vector<uint8_t> cifar_byte_vec;
    cifar_byte_vec.reserve(3072);
    std::copy_n(cifar_data, 3072, std::back_inserter(cifar_byte_vec));
    cifar_data++;
    cifar_labels.push_back(label);
    std::vector<double> cifar_double_vec(cifar_byte_vec.begin(), cifar_byte_vec.end());
    cifar_vecs.push_back(cifar_double_vec);
  }
  normalize_cifar(cifar_vecs);

  std::vector<std::pair<int, std::vector<double>>> items;
  for(int i = 0; i < 10000; i++) {
    std::pair<int, std::vector<double>> label_vec
        = std::pair<int, std::vector<double>>(cifar_labels[i], cifar_vecs[i]);
    items.push_back(label_vec);
  }
  return items;
}

void send_predictions(QueryProcessor &qp,
                      std::vector<std::pair<int, std::vector<double>>> &cifar_data,
                      std::shared_ptr<metrics::RatioCounter> accuracy_ratio) {

  for (int j = 0; j < NUM_BATCHES; j++) {
    std::vector<boost::future<Response>> futures;
    std::vector<int> labels;
    for (int i = 0; i < NUM_REQUESTS_PER_BATCH; i++) {
      int index = std::rand() % 10000;
      std::pair<int, std::vector<double>> label_vec = cifar_data[index];
      std::shared_ptr<Input> cifar_input = std::make_shared<DoubleVector>(label_vec.second);
      // CHANGE LAST PARAMETER TO QUERY CONSTRUCTOR, WRONG MODEL NAMES!
      boost::future<Response> future =
          qp.predict({"test", 0, cifar_input, 100000, "EXP3", {std::make_pair(SKLEARN_MODEL_NAME, 1)}});
      futures.push_back(std::move(future));
      labels.push_back(label_vec.first);
    }
    std::shared_ptr<std::atomic_int> completed = std::make_shared<std::atomic_int>(0);
    std::pair<boost::future<void>, std::vector<boost::future<Response>>> results =
        future::when_all(std::move(futures), completed);
    results.first.get();
    for(int i = 0; i < results.second.size(); i++) {
      boost::future<Response>& future = results.second[i];
      double label = static_cast<double>(labels[i]);
      double pred = future.get().output_.y_hat_;
      log_info_formatted("BENCH", "PRED: {}, ACTUAL: {}", pred, label);
      if(pred == label) {
        accuracy_ratio->increment(1,1);
      } else {
        accuracy_ratio->increment(0,1);
      }
    }
    std::this_thread::sleep_for(std::chrono::seconds(SLEEP_PER_BATCH_SECONDS));
  }
}

int main() {
  get_config().ready();
  QueryProcessor qp;
  std::this_thread::sleep_for(std::chrono::seconds(3));
  std::shared_ptr<metrics::RatioCounter> accuracy_ratio =
      metrics::MetricsRegistry::get_metrics().create_ratio_counter("accuracy");
  std::vector<std::pair<int, std::vector<double>>> cifar_data = load_cifar();
  std::vector<std::thread> threads;
  for (int i = 0; i < NUM_THREADS; i++) {
    std::thread thread([&]() {
      send_predictions(qp, cifar_data, accuracy_ratio);
    });
    threads.push_back(std::move(thread));
  }
  for (auto &thread : threads) {
    thread.join();
  }
  std::string metrics = metrics::MetricsRegistry::get_metrics().report_metrics();
  log_info("BENCH", metrics);
}
