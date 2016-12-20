#include <thread>
#include <unordered_map>

#include <clipper/containers.hpp>
#include <clipper/datatypes.hpp>
#include <clipper/rpc_service.hpp>

void benchmark() {
  auto containers = std::make_shared<clipper::ActiveContainers>();
  clipper::RPCService rpc_service(containers);

  rpc_service.start("127.0.0.1", 8000);
  usleep(5000000);

  std::unordered_map<int, long> times_map;
  bool shutdown = false;
  long total = 0;

  std::thread([&] {
    while (true) {
      if (shutdown) {
        return;
      }
      long end = std::chrono::duration_cast<std::chrono::milliseconds>(
                     std::chrono::system_clock::now().time_since_epoch())
                     .count();
      vector<clipper::RPCResponse> responses = rpc_service.try_get_responses(1);
      if (responses.empty()) {
        continue;
      }
      std::unordered_map<int, long>::const_iterator start_time =
          times_map.find(responses.front().first);
      if (start_time != times_map.end()) {
        long elapsed = end - start_time->second;
        total += elapsed;
        printf("%d ms\n", (int)elapsed);
      }
    }
  }).detach();

  // broken up messages

//  clipper::PredictionRequest request(clipper::InputType::Strings);
//  for (int num_inputs = 0; num_inputs < 500; ++num_inputs) {
//    std::vector<std::string> cur_data;
//    if((num_inputs % 3) == 0) {
//      cur_data.push_back(std::string("CAT"));
//      cur_data.push_back(std::string("HORSE"));
//    } else if((num_inputs % 3) == 1) {
//      cur_data.push_back(std::string("DOG"));
//    } else {
//      cur_data.push_back(std::string("COW"));
//      cur_data.push_back(std::string("FOX"));
//      cur_data.push_back(std::string("AARDVARK"));
//      cur_data.push_back(std::string("RABBIT"));
//      cur_data.push_back(std::string("FROG"));
//    }
//    std::shared_ptr<clipper::StringVector> input = std::make_shared<clipper::StringVector>(cur_data);
//    request.add_input(input);
//  }

  clipper::PredictionRequest request(clipper::InputType::Doubles);

  for (int num_inputs = 0; num_inputs < 500; ++num_inputs) {
    std::vector<double> cur_data;
    for (int j = 0; j < 784; ++j) {
      cur_data.push_back(j);
    }
    std::shared_ptr<clipper::DoubleVector> input = std::make_shared<clipper::DoubleVector>(cur_data);
    request.add_input(input);
  }

  // one giant message, batch=1
  // no difference in time
  //  std::vector<const std::vector<uint8_t>> data;
  //  std::vector<double> cur_data;
  //  for (int num_inputs = 0; num_inputs < 500; ++num_inputs) {
  //    for (int j = 0; j < 784; ++j) {
  //      cur_data.push_back(j);
  //    }
  //  }
  //  clipper::DoubleVector d(cur_data);
  //  data.push_back(d.serialize());

  int num_total_messages = 300;
  for (int i = 0; i < num_total_messages; i++) {
    //    uint8_t *data = (uint8_t *) malloc(8 * 784 * 500);
    //    std::vector<uint8_t> msg = std::vector<uint8_t>(data, data + (8 * 784
    //    * 500));
    long start = std::chrono::duration_cast<std::chrono::milliseconds>(
                     std::chrono::system_clock::now().time_since_epoch())
                     .count();
    //    int container_id =
    //    containers->get_model_replicas_snapshot(containers->get_known_models()[0])[0]->container_id_;
    int id = rpc_service.send_message(request.serialize(), 0);
    //    std::cout << "send message " << id << std::endl;
    times_map.emplace(id, start);
    usleep(50000);
  }
  //    usleep(10000000);
  shutdown = true;
  rpc_service.stop();
  printf("%f ms\n", ((double)total) / num_total_messages);
}

// void simple_test() {
//  clipper::RPCService rpc_service;
//  rpc_service.start("127.0.0.1", 7000);
//  usleep(5000000);
//  const char *msg = std::string("cat").c_str();
//  rpc_service.send_message(vector<uint8_t>((uint8_t *) msg, (uint8_t *) msg +
//  3), 0);
//
//  vector<clipper::RPCResponse> responses;
//  while(true) {
//    responses = rpc_service.try_get_responses(1);
//    if (responses.empty()) {
//      continue;
//    }
//    std::vector<uint8_t> response_msg = responses.front().second;
//    printf("%s\n", response_msg.data());
//    rpc_service.stop();
//    return;
//  }
//}

int main() {
  benchmark();
  // simple_test();
  return 0;
}
