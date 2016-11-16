#ifndef CLIPPER_RPC_SERVICE_HPP
#define CLIPPER_RPC_SERVICE_HPP

#include <string>
#include <vector>
#include <list>
#include <clipper/util.hpp>
#include <zmq.hpp>
#include <boost/bimap.hpp>
#include <queue>

using zmq::socket_t;
using std::string;
using std::shared_ptr;
using std::vector;
using std::list;

namespace clipper {

using RPCResponse = std::pair<const int, vector<uint8_t>>;
// Tuple of container_id, message_id, pointer to data, data length
using RPCRequest = std::tuple<const int, const int, const uint8_t *, size_t>;

class RPCService {
 public:
  explicit RPCService();
  ~RPCService();
  RPCService(const RPCService &) = delete;
  RPCService &operator=(const RPCService &) = delete;
  vector<RPCResponse> try_get_responses(const int max_num_responses);
  /**
   * Starts the RPC Service. This must be called explicitly, as it is not
   * invoked during construction.
   */
  void start(const string ip, const int port);
  /**
   * Stops the RPC Service. This is called implicitly within the RPCService destructor.
   */
  void stop();
  int send_message(const vector<uint8_t> &msg, const int container_id);

 private:
  void manage_service(const string address,
                      shared_ptr<Queue<RPCRequest>> request_queue,
                      shared_ptr<Queue<RPCResponse>> response_queue,
                      const bool &active);
  /**
   * @return The id of the sent message, used for match the correct response
   * If the service is active, this id is non-negative. Otherwise, it is -1.
   */
  void send_messages(socket_t &socket,
                     shared_ptr<Queue<RPCRequest>> request_queue,
                     boost::bimap<int, vector<uint8_t>> &connections);
  void receive_message(socket_t &socket,
                       shared_ptr<Queue<RPCResponse>> response_queue,
                       boost::bimap<int, vector<uint8_t>> &connections,
                       int &container_id);
  void shutdown_service(const string address, socket_t &socket);
  shared_ptr<Queue<RPCRequest>> request_queue_;
  shared_ptr<Queue<RPCResponse>> response_queue_;
  // Flag indicating whether rpc service is active
  bool active_ = false;
  // The next available message id
  int message_id_ = 0;
};

}// namespace clipper

#endif //CLIPPER_RPC_SERVICE_HPP
