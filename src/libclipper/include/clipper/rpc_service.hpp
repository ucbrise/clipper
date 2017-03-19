#ifndef CLIPPER_RPC_SERVICE_HPP
#define CLIPPER_RPC_SERVICE_HPP

#include <list>
#include <queue>
#include <string>
#include <vector>

#include <boost/bimap.hpp>
#include <redox.hpp>
#include <zmq.hpp>

#include <clipper/containers.hpp>
#include <clipper/datatypes.hpp>
#include <clipper/metrics.hpp>
#include <clipper/util.hpp>

using zmq::socket_t;
using std::string;
using std::shared_ptr;
using std::vector;
using std::list;

namespace clipper {

namespace rpc {

const std::string LOGGING_TAG_RPC = "RPC";

using RPCResponse = std::pair<const int, vector<uint8_t>>;
/// Tuple of zmq_connection_id, message_id, vector of messages, creation time
using RPCRequest =
    std::tuple<const int, const int, const std::vector<std::vector<uint8_t>>,
               const long>;

class RPCService {
 public:
  explicit RPCService();
  ~RPCService();
  // Disallow copy
  RPCService(const RPCService &) = delete;
  RPCService &operator=(const RPCService &) = delete;
  vector<RPCResponse> try_get_responses(const int max_num_responses);
  /**
   * Starts the RPC Service. This must be called explicitly, as it is not
   * invoked during construction.
   */
  void start(
      const string ip, const int port,
      std::function<void(VersionedModelId, int)> &&container_ready_callback,
      std::function<void(RPCResponse)> &&new_response_callback);
  /**
   * Stops the RPC Service. This is called implicitly within the RPCService
   * destructor.
   */
  void stop();

  /*
  * Send message takes ownership of the msg data because the caller cannot
  * know when the message will actually be sent.
  *
  * \param `msg`: A vector of individual messages to send to this container.
  * The messages will be sent as a single, multi-part ZeroMQ message so
  * it is very efficient.
  */
  int send_message(const std::vector<std::vector<uint8_t>> msg,
                   const int zmq_connection_id);

 private:
  void manage_service(const string address);
  /**
   * \return The id of the sent message, used for match the correct response
   * If the service is active, this id is non-negative. Otherwise, it is -1.
   */
  void send_messages(socket_t &socket,
                     boost::bimap<int, vector<uint8_t>> &connections);

  void receive_message(
      socket_t &socket, boost::bimap<int, vector<uint8_t>> &connections,
      // This is a mapping from a ZMQ connection id
      // to metadata associated with the container using
      // this connection. Values are pairs of
      // model id and integer replica id
      std::unordered_map<std::vector<uint8_t>, std::pair<VersionedModelId, int>,
                         std::function<size_t(const std::vector<uint8_t> &vec)>>
          &connections_containers_map,
      int &zmq_connection_id, std::shared_ptr<redox::Redox> redis_connection);
  void shutdown_service(socket_t &socket);
  std::thread rpc_thread_;
  shared_ptr<Queue<RPCRequest>> request_queue_;
  shared_ptr<Queue<RPCResponse>> response_queue_;
  // Flag indicating whether rpc service is active
  std::atomic_bool active_;
  // The next available message id
  int message_id_ = 0;
  std::unordered_map<VersionedModelId, int, decltype(&versioned_model_hash)>
      replica_ids_;
  std::shared_ptr<metrics::Histogram> msg_queueing_hist_;

  std::function<void(VersionedModelId, int)> container_ready_callback_;
  std::function<void(RPCResponse)> new_response_callback_;
};

}  // namespace rpc

}  // namespace clipper

#endif  // CLIPPER_RPC_SERVICE_HPP
