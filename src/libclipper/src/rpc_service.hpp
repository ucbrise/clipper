#ifndef CLIPPER_RPC_SERVICE_HPP
#define CLIPPER_RPC_SERVICE_HPP

#include <string>
#include <vector>

namespace clipper {

using RPCResponse = std::pair<int, std::vector<uint8_t>>;
// Tuple of container_id, pointer to data, data length
using RPCRequest = std::tuple<int, uint8_t *, size_t>;

class RPCService {
 public:
  explicit RPCService(shared_ptr <Queue<RPCResponse>> response_queue);
  ~RPCService();
  RPCService(const RPCService &) = delete;
  RPCService &operator=(const RPCService &) = delete;
  void start(const std::string ip, const int port);
  void stop();
  const int send_message(const std::vector<uint8_t> &msg, const int container_id);

 private:
  void manage_service(const std::string address,
                      shared_ptr <Queue<RPCRequest>> send_queue,
                      shared_ptr <Queue<RPCResponse>> response_queue,
                      const bool &shutdown);
  shared_ptr <Queue<RPCRequest>> send_queue_;
  shared_ptr <Queue<RPCResponse>> response_queue_;
  // Flag indicating whether rpc service has been shutdown
  bool shutdown_ = false;

};

}// namespace clipper

#endif //CLIPPER_RPC_SERVICE_HPP
