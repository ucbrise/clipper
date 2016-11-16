#include <unordered_map>
#include <thread>
#include "rpc_service.hpp"

void benchmark() {
  auto response_queue = std::make_shared<std::queue<clipper::RPCResponse>>();
  auto response_lock = std::make_shared<std::mutex>();
  clipper::RPCService rpc_service(response_queue, response_lock);
  rpc_service.start("127.0.0.1", 7000);
  usleep(5000000);

  std::unordered_map<int, long> times_map;
  bool shutdown = false;
  long total = 0;

  std::thread([&] {
    while(true) {
      if(shutdown) {
        return;
      }
      if(response_queue->empty()) {
        continue;
      }
      long end = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch()).count();
      response_lock->lock();
      while(!response_queue->empty()) {
        clipper::RPCResponse response = response_queue->front();
        response_queue->pop();
        std::unordered_map<int,long>::const_iterator start_time = times_map.find(response.first);
        if(start_time != times_map.end()) {
          long elapsed = end - start_time->second;
          total += elapsed;
          printf("%d\n", (int) elapsed);
        }
      }
      response_lock->unlock();
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
  printf("%f\n", ((double) total)/300);
}

void simple_test() {
  auto response_queue = std::make_shared<std::queue<clipper::RPCResponse>>();
  auto response_lock = std::make_shared<std::mutex>();
  clipper::RPCService rpc_service(response_queue, response_lock);
  rpc_service.start("127.0.0.1", 7000);
  usleep(5000000);
  const char *msg = std::string("cat").c_str();
  rpc_service.send_message(vector<uint8_t>((uint8_t *) msg, (uint8_t *) msg + 3), 0);
  while(response_queue->empty()) {}
  clipper::RPCResponse response = response_queue->front();
  std::vector<uint8_t> response_msg = response.second;
  printf("%s\n", response_msg.data());
}

int main() {
  //benchmark();
  simple_test();
  return 0;
}
