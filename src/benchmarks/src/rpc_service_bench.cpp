#include <cstdlib>
#include <iostream>
#include <thread>
#include <unordered_map>

#include <cxxopts.hpp>

#include <clipper/config.hpp>
#include <clipper/datatypes.hpp>
#include <clipper/logging.hpp>
#include <clipper/memory.hpp>
#include <clipper/metrics.hpp>
#include <clipper/redis.hpp>
#include <clipper/rpc_service.hpp>
#include <clipper/task_executor.hpp>
#include <clipper/util.hpp>

using namespace clipper;

const std::string LOGGING_TAG_RPC_BENCH = "RPCBENCH";

// taken from http://stackoverflow.com/a/12468109/814642
std::string gen_random_string(size_t length) {
  std::srand(time(NULL));
  auto randchar = []() -> char {
    const char charset[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    const size_t max_index = (sizeof(charset) - 1);
    return charset[std::rand() % max_index];
  };
  std::string str(length, 0);
  std::generate_n(str.begin(), length, randchar);
  return str;
}

std::string get_thread_id() {
  std::stringstream ss;
  ss << std::this_thread::get_id();
  return ss.str();
}

template <typename T, class N>
std::vector<std::unique_ptr<PredictionData>> get_primitive_inputs(
    int message_size, int input_len, InputType type) {
  std::vector<std::unique_ptr<PredictionData>> inputs;
  for (int k = 0; k < message_size; ++k) {
    UniquePoolPtr<T> input_data = memory::allocate_unique<T>(input_len);
    T *input_data_raw = input_data.get();
    for (int j = 0; j < input_len; ++j) {
      if (type == InputType::Bytes) {
        uint8_t *bytes = reinterpret_cast<uint8_t *>(&j);
        for (int i = 0; i < (int)(sizeof(int) / sizeof(uint8_t)); i++) {
          input_data_raw[j] = *(bytes + i);
        }
      } else {
        input_data_raw[j] = static_cast<T>(j);
      }
    }
    std::unique_ptr<PredictionData> input =
        std::make_unique<N>(std::move(input_data), input_len);
    inputs.push_back(std::move(input));
  }
  return inputs;
}

rpc::PredictionRequest generate_bytes_request(int message_size) {
  std::vector<std::unique_ptr<PredictionData>> inputs =
      get_primitive_inputs<uint8_t, ByteVector>(message_size, 784,
                                                InputType::Bytes);
  rpc::PredictionRequest request(std::move(inputs), InputType::Bytes);
  return request;
}

rpc::PredictionRequest generate_floats_request(int message_size) {
  std::vector<std::unique_ptr<PredictionData>> inputs =
      get_primitive_inputs<float, FloatVector>(message_size, 784,
                                               InputType::Floats);
  rpc::PredictionRequest request(std::move(inputs), InputType::Floats);
  return request;
}

rpc::PredictionRequest generate_ints_request(int message_size) {
  std::vector<std::unique_ptr<PredictionData>> inputs =
      get_primitive_inputs<int, IntVector>(message_size, 784, InputType::Ints);
  rpc::PredictionRequest request(std::move(inputs), InputType::Ints);
  return request;
}

rpc::PredictionRequest generate_doubles_request(int message_size) {
  std::vector<std::unique_ptr<PredictionData>> inputs =
      get_primitive_inputs<double, DoubleVector>(message_size, 784,
                                                 InputType::Doubles);
  rpc::PredictionRequest request(std::move(inputs), InputType::Doubles);
  return request;
}

rpc::PredictionRequest generate_string_request(int message_size) {
  rpc::PredictionRequest request(InputType::Strings);
  for (int i = 0; i < message_size; ++i) {
    std::string str = gen_random_string(150);
    std::unique_ptr<PredictionData> input = to_serializable_string(str);
    request.add_input(std::move(input));
  }
  return request;
}

rpc::PredictionRequest create_request(InputType input_type, int message_size) {
  switch (input_type) {
    case InputType::Strings: return generate_string_request(message_size);
    case InputType::Doubles: return generate_doubles_request(message_size);
    case InputType::Floats: return generate_floats_request(message_size);
    case InputType::Bytes: return generate_bytes_request(message_size);
    case InputType::Ints: return generate_ints_request(message_size);
    case InputType::Invalid:
    default: throw std::invalid_argument("Unsupported input type");
  }
}

class Benchmarker {
 public:
  Benchmarker(int num_messages, int message_size, InputType input_type)
      : num_messages_(num_messages),
        rpc_(std::make_unique<rpc::RPCService>()),
        request_(create_request(input_type, message_size)) {}

  void start() {
    Config &conf = get_config();
    rpc_->start("*", conf.get_rpc_service_port(),
                [](VersionedModelId, int) {},
                [this](rpc::RPCResponse &response) {
                  on_response_recv(std::move(response));
                },
                [](VersionedModelId, int) {});

    msg_latency_hist_ =
        metrics::MetricsRegistry::get_metrics().create_histogram(
            "rpc_bench_msg_latency", "milliseconds", 8260);
    throughput_meter_ = metrics::MetricsRegistry::get_metrics().create_meter(
        "rpc_bench_throughput");

    while (!redis_connection_.connect(conf.get_redis_address(),
                                      conf.get_redis_port())) {
      log_error(LOGGING_TAG_RPC_BENCH, "RPCBench failed to connect to redis",
                "Retrying in 1 second...");
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    while (!redis_subscriber_.connect(conf.get_redis_address(),
                                      conf.get_redis_port())) {
      log_error(LOGGING_TAG_RPC_BENCH,
                "RPCBench subscriber failed to connect to redis",
                "Retrying in 1 second...");
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    redis::send_cmd_no_reply<std::string>(
        redis_connection_, {"CONFIG", "SET", "notify-keyspace-events", "AKE"});
    redis::subscribe_to_container_changes(
        redis_subscriber_,
        // event_type corresponds to one of the Redis event types
        // documented in https://redis.io/topics/notifications.
        [this](const std::string &key, const std::string &event_type) {
          if (event_type == "hset") {
            auto container_info =
                redis::get_container_by_key(redis_connection_, key);
            benchmark_container_id_ =
                std::stoi(container_info["zmq_connection_id"]);

            // SEND FIRST MESSAGE
            send_message();
          }
        });
  }

  Benchmarker(const Benchmarker &other) = delete;
  Benchmarker &operator=(const Benchmarker &other) = delete;

  Benchmarker(Benchmarker &&other) = default;
  Benchmarker &operator=(Benchmarker &&other) = default;
  ~Benchmarker() {
    std::unique_lock<std::mutex> l(bench_completed_cv_mutex_);
    bench_completed_cv_.wait(l, [this]() { return bench_completed_ == true; });
  }

  void send_message() {
    cur_msg_start_time_millis_ =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();
    cur_message_id_ =
        rpc_->send_message(request_.serialize(), benchmark_container_id_);
  }

  void on_response_recv(rpc::RPCResponse response) {
    long recv_time_millis =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();
    if (response.first != cur_message_id_) {
      std::stringstream ss;
      ss << "Response message ID ";
      ss << response.first;
      ss << " did not match in flight message ID ";
      ss << cur_message_id_;
      throw std::logic_error(ss.str());
    }
    long message_duration_millis =
        recv_time_millis - cur_msg_start_time_millis_;
    msg_latency_hist_->insert(message_duration_millis);
    throughput_meter_->mark(1);
    messages_completed_ += 1;

    if (messages_completed_ < num_messages_) {
      send_message();
    } else {
      bench_completed_ = true;
      bench_completed_cv_.notify_all();
    }
  }

  std::condition_variable_any bench_completed_cv_;
  std::mutex bench_completed_cv_mutex_;
  std::atomic<bool> bench_completed_{false};

 private:
  std::shared_ptr<metrics::Histogram> msg_latency_hist_;
  std::shared_ptr<metrics::Meter> throughput_meter_;
  int num_messages_;
  redox::Redox redis_connection_;
  redox::Subscriber redis_subscriber_;
  std::atomic<int> messages_completed_{0};
  std::unique_ptr<rpc::RPCService> rpc_;
  std::atomic<long> cur_msg_start_time_millis_;
  std::atomic<int> benchmark_container_id_;
  rpc::PredictionRequest request_;
  uint32_t cur_message_id_;
};

int main(int argc, char *argv[]) {
  cxxopts::Options options("rpc_bench", "Clipper RPC Benchmark");
  // clang-format off
  options.add_options()
    ("redis_ip", "Redis address",
        cxxopts::value<std::string>()->default_value("localhost"))
    ("redis_port", "Redis port",
        cxxopts::value<int>()->default_value(std::to_string(DEFAULT_REDIS_PORT)))
    ("m,num_messages", "Number of messages to send",
        cxxopts::value<int>()->default_value("100"))
    ("s,message_size", "Number of inputs per message",
        cxxopts::value<int>()->default_value("500"))
    ("rpc_service_port", "RPCService's port",
        cxxopts::value<int>()->default_value(std::to_string(DEFAULT_RPC_SERVICE_PORT)))
    ("input_type", "Can be bytes, ints, floats, doubles, or strings",
        cxxopts::value<std::string>()->default_value("doubles"));
  // clang-format on
  options.parse(argc, argv);

  clipper::Config &conf = clipper::get_config();
  conf.set_redis_address(options["redis_ip"].as<std::string>());
  conf.set_redis_port(options["redis_port"].as<int>());
  conf.set_rpc_service_port(options["rpc_service_port"].as<int>());
  conf.ready();
  InputType input_type =
      clipper::parse_input_type(options["input_type"].as<std::string>());
  Benchmarker benchmarker(options["num_messages"].as<int>(),
                          options["message_size"].as<int>(), input_type);
  benchmarker.start();

  std::unique_lock<std::mutex> l(benchmarker.bench_completed_cv_mutex_);
  benchmarker.bench_completed_cv_.wait(
      l, [&benchmarker]() { return benchmarker.bench_completed_ == true; });
  metrics::MetricsRegistry &registry = metrics::MetricsRegistry::get_metrics();
  std::string metrics_report = registry.report_metrics();
  log_info(LOGGING_TAG_RPC_BENCH, "METRICS", metrics_report);
  return 0;
}
