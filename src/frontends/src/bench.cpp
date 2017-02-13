#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

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
constexpr long NUM_THREADS = 1;
constexpr long NUM_REQUESTS_PER_BATCH = 10;
constexpr long SLEEP_PER_BATCH_SECONDS = 0;

std::vector<std::pair<int, std::vector<double>>> load_cifar() {
  std::ifstream cifar_file("../../../bench/data/cifar-10-batches-bin/data_batch_1.bin", std::ios::binary);
  std::istreambuf_iterator<char> cifar_data(cifar_file);
  std::vector<std::pair<int, std::vector<double>>> items;
  for(int i = 0; i < 10000; i++) {
    int label = static_cast<int>(*cifar_data);
    cifar_data++;
    std::vector<double> cifar_vec;
    cifar_vec.reserve(3072);
    std::copy_n(cifar_data, 3072, std::back_inserter(cifar_vec));
    cifar_data++;
    //std::vector<double> double_vec(cifar_vec.begin(), cifar_vec.end());
    std::pair<int, std::vector<double>> label_vec(label, cifar_vec);
    items.push_back(label_vec);
  }
  return items;
}

void send_predictions(QueryProcessor &qp, std::vector<std::pair<int, std::vector<double>>>& cifar_data) {
  for(int j = 0; j < 10; j++) {
    log_info_formatted("BENCH", "Sending batch: {}", j);
    std::vector<boost::future<Response>> futures;
    for (int i = 0; i < NUM_REQUESTS_PER_BATCH; i++) {
      int index = std::rand() % 10000;
      std::pair<int, std::vector<double>> label_vec = cifar_data[index];
      std::shared_ptr<Input> cifar_input = std::make_shared<DoubleVector>(label_vec.second);
      // CHANGE LAST PARAMETER TO QUERY CONSTRUCTOR, WRONG MODEL NAMES!
      boost::future<Response> future =
          qp.predict({"test", 0, cifar_input, 20000, "EXP3", {std::make_pair(SKLEARN_MODEL_NAME, 1)}});
      futures.push_back(std::move(future));
    }
    std::shared_ptr<std::atomic_int> completed = std::make_shared<std::atomic_int>(0);
    std::pair<boost::future<void>, std::vector<boost::future<Response>>> results =
        future::when_all(std::move(futures), completed);
    results.first.get();
    std::this_thread::sleep_for(std::chrono::seconds(SLEEP_PER_BATCH_SECONDS));
  }
}

int main() {
  get_config().ready();
  QueryProcessor qp;
  std::this_thread::sleep_for(std::chrono::seconds(3));
  std::vector<std::pair<int, std::vector<double>>> cifar_data = load_cifar();
  std::vector<std::thread> threads;
  for(int i = 0; i < NUM_THREADS; i++) {
    std::thread thread([&]() {
      send_predictions(qp, cifar_data);
    });
    threads.push_back(std::move(thread));
  }
  for(auto& thread : threads) {
    thread.join();
  }
  std::string metrics = metrics::MetricsRegistry::get_metrics().report_metrics();
  log_info("BENCH", metrics);
}
