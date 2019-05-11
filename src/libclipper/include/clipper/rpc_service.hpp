#ifndef CLIPPER_RPC_SERVICE_HPP
#define CLIPPER_RPC_SERVICE_HPP

#include <chrono>
#include <list>
#include <queue>
#include <string>
#include <vector>

#include <concurrentqueue.h>
#include <boost/bimap.hpp>
#include <redox.hpp>
#include <zmq.hpp>

#include <clipper/containers.hpp>
#include <clipper/datatypes.hpp>
#include <clipper/metrics.hpp>
#include <clipper/util.hpp>

using std::list;
using std::shared_ptr;
using std::string;
using std::vector;
using zmq::socket_t;

namespace clipper {

namespace rpc {

const std::string LOGGING_TAG_RPC = "RPC";
static constexpr uint32_t RPC_VERSION = 3;

/// Tuple of msg_id, vector of model outputs
using RPCResponse = std::pair<uint32_t, std::vector<ByteBuffer>>;
/// Tuple of zmq_connection_id, message_id, vector of messages, creation time
using RPCRequest =
    std::tuple<uint32_t, uint32_t, std::vector<ByteBuffer>, long>;

// Tuple of model id, replica id, and last activity time associated
// with each container that has connected (sent metadata) to clipper
using ConnectedContainerInfo =
    std::tuple<VersionedModelId, int, std::chrono::system_clock::time_point>;

enum class RPCEvent {
  SentHeartbeat = 1,
  ReceivedHeartbeat = 2,
  SentContainerMetadata = 3,
  ReceivedContainerMetadata = 4,
  SentContainerContent = 5,
  ReceivedContainerContent = 6
};

enum class MessageType {
  NewContainer = 0,
  ContainerContent = 1,
  Heartbeat = 2
};

enum class HeartbeatType { KeepAlive = 0, RequestContainerMetadata = 1 };

class RPCDataStore {
 public:
  void add_data(SharedPoolPtr<void> data);
  void remove_data(void *data);

 private:
  std::mutex mtx_;
  std::unordered_map<void *, SharedPoolPtr<void>> data_items_;
};

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
      std::function<void(RPCResponse &)> &&new_response_callback,
      std::function<void(VersionedModelId, int)> &&inactive_container_callback);
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
  int send_message(std::vector<ByteBuffer> msg,
                   const uint32_t zmq_connection_id);

 private:
  void manage_service(const string address);

  void check_container_activity(
      boost::bimap<int, vector<uint8_t>> &connections,
      std::unordered_map<std::vector<uint8_t>, ConnectedContainerInfo,
                         std::function<size_t(const std::vector<uint8_t> &vec)>>
          &connections_containers_map);

  void send_messages(socket_t &socket,
                     boost::bimap<int, vector<uint8_t>> &connections);

  /**
   * Function called by ZMQ after it finishes
   * sending data that it owned as a result
   * of a call to `zmq_msg_init_data`
   */
  static void zmq_continuation(void *data, void *hint);

  void document_receive_time(
      std::unordered_map<std::vector<uint8_t>, ConnectedContainerInfo,
                         std::function<size_t(const std::vector<uint8_t> &vec)>>
          &connections_containers_map,
      const vector<uint8_t> connection_id);

  void receive_message(
      socket_t &socket, boost::bimap<int, vector<uint8_t>> &connections,
      // This is a mapping from a ZMQ connection id
      // to metadata associated with the container using
      // this connection. Values are pairs of
      // model id and integer replica id
      std::unordered_map<std::vector<uint8_t>, ConnectedContainerInfo,
                         std::function<size_t(const std::vector<uint8_t> &vec)>>
          &connections_containers_map,
      uint32_t &zmq_connection_id,
      std::shared_ptr<redox::Redox> redis_connection);

  void send_heartbeat_response(socket_t &socket,
                               const vector<uint8_t> &connection_id,
                               bool request_container_metadata);

  void shutdown_service(socket_t &socket);
  std::thread rpc_thread_;
  shared_ptr<moodycamel::ConcurrentQueue<RPCRequest>> request_queue_;
  // Flag indicating whether rpc service is active
  std::atomic_bool active_;
  // The next available message id
  int message_id_ = 0;
  std::chrono::system_clock::time_point last_activity_check_time_;
  std::unordered_map<VersionedModelId, int> replica_ids_;
  std::shared_ptr<metrics::Histogram> msg_queueing_hist_;
  std::function<void(VersionedModelId, int)> container_ready_callback_;
  std::function<void(RPCResponse &)> new_response_callback_;

  RPCDataStore outbound_data_store_;
  std::function<void(VersionedModelId, int)> inactive_container_callback_;

  static constexpr int INITIAL_REPLICA_ID_SIZE = 100;
  static constexpr long CONTAINER_ACTIVITY_TIMEOUT_MILLS = 30000;
  static constexpr long CONTAINER_EXISTENCE_CHECK_FREQUENCY_MILLS = 10000;
};

}  // namespace rpc

}  // namespace clipper

#endif  // CLIPPER_RPC_SERVICE_HPP
