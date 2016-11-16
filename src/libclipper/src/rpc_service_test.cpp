#include <unordered_map>
#include <thread>
#include "rpc_service.hpp"

void benchmark() {
  clipper::RPCService rpc_service;
  rpc_service.start("127.0.0.1", 7000);
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
          std::chrono::system_clock::now().time_since_epoch()).count();
      vector<clipper::RPCResponse> responses = rpc_service.try_get_responses(1);
      if(responses.empty()) {
        continue;
      }
      std::unordered_map<int, long>::const_iterator start_time = times_map.find(responses.front().first);
      if (start_time != times_map.end()) {
        long elapsed = end - start_time->second;
        total += elapsed;
        printf("%d\n", (int) elapsed);
      }
    }
  }).detach();

  for (int i = 0; i < 300; i++) {
    uint8_t *data = (uint8_t *) malloc(8 * 784 * 500);
    std::vector<uint8_t> msg = std::vector<uint8_t>(data, data + (8 * 784 * 500));
    long start = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    int id = rpc_service.send_message(msg, 0);
    times_map.emplace(id, start);
    usleep(100000);
    free(data);
  }
  shutdown = true;
  rpc_service.stop();
  printf("%f\n", ((double) total) / 300);
}

void simple_test() {
  clipper::RPCService rpc_service;
  rpc_service.start("127.0.0.1", 7000);
  usleep(5000000);
  const char *msg = std::string("cat").c_str();
  rpc_service.send_message(vector<uint8_t>((uint8_t *) msg, (uint8_t *) msg + 3), 0);

  vector<clipper::RPCResponse> responses;
  while(true) {
    responses = rpc_service.try_get_responses(1);
    if (responses.empty()) {
      continue;
    }
    std::vector<uint8_t> response_msg = responses.front().second;
    printf("%s\n", response_msg.data());
    rpc_service.stop();
    return;
  }
}

int main() {
  benchmark();
  //simple_test();
  return 0;
}
