#include <thread>
#include <unordered_map>

#include <clipper/datatypes.hpp>
#include <clipper/rpc_service.hpp>
#include <clipper/logging.hpp>

void await_responses(std::shared_ptr<clipper::rpc::RPCService> rpc_service,
                     std::shared_ptr<std::unordered_map<int, long>> times_map,
                     bool &shutdown, long &total_time_elapsed) {
  std::thread([&] {
    while (true) {
      if (shutdown) {
        return;
      }
      long end = std::chrono::duration_cast<std::chrono::milliseconds>(
                     std::chrono::system_clock::now().time_since_epoch())
                     .count();
      vector<clipper::rpc::RPCResponse> responses =
          rpc_service->try_get_responses(1);
      if (responses.empty()) {
        continue;
      }
      std::unordered_map<int, long>::const_iterator start_time =
          times_map->find(responses.front().first);
      if (start_time != times_map->end()) {
        long elapsed = end - start_time->second;
        total_time_elapsed += elapsed;
        printf("%d ms\n", (int)elapsed);
      }
    }
  }).detach();
}

void send_request_repeated(
    int container_id, int num_iterations,
    clipper::rpc::PredictionRequest &request,
    std::shared_ptr<clipper::rpc::RPCService> rpc_service,
    std::shared_ptr<std::unordered_map<int, long>> times_map) {
  for (int i = 0; i < num_iterations; i++) {
    long start = std::chrono::duration_cast<std::chrono::milliseconds>(
                     std::chrono::system_clock::now().time_since_epoch())
                     .count();
    int id = rpc_service->send_message(request.serialize(), container_id);
    times_map->emplace(id, start);
    usleep(50000);
  }
}

template <typename T, class N>
std::vector<std::shared_ptr<clipper::Input>> get_primitive_inputs(
    clipper::InputType type, std::vector<T> data_vector,
    std::vector<std::shared_ptr<N>> input_vector) {
  input_vector.clear();
  std::vector<std::shared_ptr<clipper::Input>> generic_input_vector;
  for (int num_inputs = 0; num_inputs < 500; ++num_inputs) {
    for (int j = 0; j < 784; ++j) {
      if (type == clipper::InputType::Bytes) {
        uint8_t *bytes = reinterpret_cast<uint8_t *>(&j);
        for (int i = 0; i < (int)(sizeof(int) / sizeof(uint8_t)); i++) {
          data_vector.push_back(*(bytes + i));
        }
      } else {
        data_vector.push_back(static_cast<T>(j));
      }
    }
    std::shared_ptr<N> input = std::make_shared<N>(data_vector);
    generic_input_vector.push_back(
        std::dynamic_pointer_cast<clipper::Input>(input));
    data_vector.clear();
  }
  return generic_input_vector;
}

void run_bytes_benchmark(
    int container_id, int num_iterations,
    std::shared_ptr<clipper::rpc::RPCService> rpc_service,
    std::shared_ptr<std::unordered_map<int, long>> times_map) {
  std::vector<uint8_t> type_vec;
  std::vector<std::shared_ptr<clipper::ByteVector>> input_vec;
  std::vector<std::shared_ptr<clipper::Input>> inputs =
      get_primitive_inputs(clipper::InputType::Bytes, type_vec, input_vec);
  clipper::rpc::PredictionRequest request(inputs, clipper::InputType::Bytes);
  send_request_repeated(container_id, num_iterations, request, rpc_service,
                        times_map);
}

void run_ints_benchmark(
    int container_id, int num_iterations,
    std::shared_ptr<clipper::rpc::RPCService> rpc_service,
    std::shared_ptr<std::unordered_map<int, long>> times_map) {
  std::vector<int> type_vec;
  std::vector<std::shared_ptr<clipper::IntVector>> input_vec;
  std::vector<std::shared_ptr<clipper::Input>> inputs =
      get_primitive_inputs(clipper::InputType::Ints, type_vec, input_vec);
  clipper::rpc::PredictionRequest request(inputs, clipper::InputType::Ints);
  send_request_repeated(container_id, num_iterations, request, rpc_service,
                        times_map);
}

void run_floats_benchmark(
    int container_id, int num_iterations,
    std::shared_ptr<clipper::rpc::RPCService> rpc_service,
    std::shared_ptr<std::unordered_map<int, long>> times_map) {
  std::vector<float> type_vec;
  std::vector<std::shared_ptr<clipper::FloatVector>> input_vec;
  std::vector<std::shared_ptr<clipper::Input>> inputs =
      get_primitive_inputs(clipper::InputType::Floats, type_vec, input_vec);
  clipper::rpc::PredictionRequest request(inputs, clipper::InputType::Floats);
  send_request_repeated(container_id, num_iterations, request, rpc_service,
                        times_map);
}

void run_doubles_benchmark(
    int container_id, int num_iterations,
    std::shared_ptr<clipper::rpc::RPCService> rpc_service,
    std::shared_ptr<std::unordered_map<int, long>> times_map) {
  std::vector<double> type_vec;
  std::vector<std::shared_ptr<clipper::DoubleVector>> input_vec;
  std::vector<std::shared_ptr<clipper::Input>> inputs =
      get_primitive_inputs(clipper::InputType::Doubles, type_vec, input_vec);
  clipper::rpc::PredictionRequest request(inputs, clipper::InputType::Doubles);
  send_request_repeated(container_id, num_iterations, request, rpc_service,
                        times_map);
}

void run_strings_benchmark(
    int container_id, int num_iterations,
    std::shared_ptr<clipper::rpc::RPCService> rpc_service,
    std::shared_ptr<std::unordered_map<int, long>> times_map) {
  clipper::rpc::PredictionRequest request(clipper::InputType::Strings);
  for (int num_inputs = 0; num_inputs < 500; ++num_inputs) {
    std::string str;
    if ((num_inputs % 3) == 0) {
      str = std::string("CAT");
    } else if ((num_inputs % 3) == 1) {
      str = std::string("DOG");
    } else {
      str = std::string("COW");
    }
    std::shared_ptr<clipper::SerializableString> input =
        std::make_shared<clipper::SerializableString>(str);
    request.add_input(input);
  }
  send_request_repeated(container_id, num_iterations, request, rpc_service,
                        times_map);
}

void run_benchmarks() {
  std::shared_ptr<clipper::rpc::RPCService> rpc_service =
      std::make_shared<clipper::rpc::RPCService>();
  std::shared_ptr<std::unordered_map<int, long>> times_map =
      std::make_shared<std::unordered_map<int, long>>();
  bool shutdown = false;
  long total_time_elapsed = 0;
  int num_iterations_per_benchmark = 300;
  int num_benchmarks = 1;

  rpc_service->start("127.0.0.1", 8000);
  usleep(5000000);

  await_responses(rpc_service, times_map, shutdown, total_time_elapsed);
  // run_bytes_benchmark(0, num_iterations_per_benchmark, rpc_service,
  // times_map);
  // run_ints_benchmark(0, num_iterations_per_benchmark, rpc_service,
  // times_map);
  // run_floats_benchmark(0, num_iterations_per_benchmark, rpc_service,
  // times_map);
  run_doubles_benchmark(0, num_iterations_per_benchmark, rpc_service,
                        times_map);
  // run_strings_benchmark(0, num_iterations_per_benchmark, rpc_service,
  //                       times_map);

  shutdown = true;
  rpc_service->stop();
  printf("%f ms\n", ((double)total_time_elapsed) /
                        (num_iterations_per_benchmark * num_benchmarks));
}

int main() {
  run_benchmarks();
  return 0;
}
