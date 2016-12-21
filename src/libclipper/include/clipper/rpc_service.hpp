#ifndef CLIPPER_RPC_SERVICE_HPP
#define CLIPPER_RPC_SERVICE_HPP

#include <list>
#include <queue>
#include <string>
#include <vector>

#include <boost/bimap.hpp>
#include <zmq.hpp>

#include <clipper/containers.hpp>
//#include <clipper/task_executor.hpp>
#include <clipper/util.hpp>

using zmq::socket_t;
using std::string;
using std::shared_ptr;
using std::vector;
using std::list;

namespace clipper {

namespace rpc {

using RPCResponse = std::pair<const int, vector<uint8_t>>;
/// Tuple of container_id, message_id, vector of messages
using RPCRequest =
std::tuple<const int, const int, const std::vector<std::vector<uint8_t>>>;

class RPCService {
 public:
  explicit RPCService(std::shared_ptr<ActiveContainers> containers);
  ~RPCService();
  // Disallow copy
  RPCService(const RPCService &) = delete;
  RPCService &operator=(const RPCService &) = delete;
  vector<RPCResponse> try_get_responses(const int max_num_responses);
  /**
   * Starts the RPC Service. This must be called explicitly, as it is not
   * invoked during construction.
   */
  void start(const string ip, const int port);
  /**
   * Stops the RPC Service. This is called implicitly within the RPCService
   * destructor.
   */
  void stop();

  /// Send message takes ownership of the msg data because the caller cannot
  /// know when the message will actually be sent.
  /// @param msg A vector of individual messages to send to this container.
  /// The messages will be sent as a single, multi-part ZeroMQ message so
  /// it is very efficient.
  int send_message(const std::vector<std::vector<uint8_t>> msg,
                   const int container_id);

 private:
  void manage_service(const string address,
                      shared_ptr<Queue<RPCRequest>> request_queue,
                      shared_ptr<Queue<RPCResponse>> response_queue,
                      shared_ptr<ActiveContainers> containers,
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
                       int &container_id,
                       std::shared_ptr<ActiveContainers> containers);

  void shutdown_service(const string address, socket_t &socket);
  shared_ptr <Queue<RPCRequest>> request_queue_;
  shared_ptr <Queue<RPCResponse>> response_queue_;
  // Flag indicating whether rpc service is active
  bool active_ = false;
  // The next available message id
  int message_id_ = 0;
  std::shared_ptr<ActiveContainers> active_containers_;
};

} // namespace rpc

}  // namespace clipper

#endif  // CLIPPER_RPC_SERVICE_HPP
