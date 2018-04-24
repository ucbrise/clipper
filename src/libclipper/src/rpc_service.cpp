#include <boost/bimap.hpp>
#include <boost/functional/hash.hpp>

#include <chrono>
#include <iostream>

#include <concurrentqueue.h>
#include <clipper/config.hpp>
#include <clipper/datatypes.hpp>
#include <clipper/logging.hpp>
#include <clipper/metrics.hpp>
#include <clipper/redis.hpp>
#include <clipper/rpc_service.hpp>
#include <clipper/task_executor.hpp>
#include <clipper/threadpool.hpp>
#include <clipper/util.hpp>
#include <redox.hpp>

using std::shared_ptr;
using std::string;
using std::vector;
using zmq::context_t;
using zmq::message_t;
using zmq::socket_t;

namespace clipper {

namespace rpc {

RPCService::RPCService()
    : request_queue_(std::make_shared<moodycamel::ConcurrentQueue<RPCRequest>>(
          sizeof(RPCRequest) * 10000)),
      response_queue_(
          std::make_shared<moodycamel::ConcurrentQueue<RPCResponse>>(
              sizeof(RPCResponse) * 10000)),
      active_(false),
      last_activity_check_time_(std::chrono::system_clock::now()),
      // The version of the unordered_map constructor that allows
      // you to specify your own hash function also requires you
      // to provide the initial size of the map. We define the initial
      // size of the map somewhat arbitrarily as 100.
      replica_ids_(std::unordered_map<VersionedModelId, int>({})) {
  msg_queueing_hist_ = metrics::MetricsRegistry::get_metrics().create_histogram(
      "internal:rpc_request_queueing_delay", "microseconds", 2056);
}

RPCService::~RPCService() { stop(); }

void RPCService::start(
    const string ip, const int port,
    std::function<void(VersionedModelId, int)> &&container_ready_callback,
    std::function<void(RPCResponse)> &&new_response_callback,
    std::function<void(VersionedModelId, int)> &&inactive_container_callback) {
  container_ready_callback_ = container_ready_callback;
  new_response_callback_ = new_response_callback;
  inactive_container_callback_ = inactive_container_callback;
  if (active_) {
    throw std::runtime_error(
        "Attempted to start RPC Service when it is already running!");
  }
  const string address = "tcp://" + ip + ":" + std::to_string(port);
  active_ = true;
  // TODO: Propagate errors from new child thread for handling
  // TODO: Explore bind vs static method call for thread creation
  rpc_thread_ = std::thread([this, address]() { manage_service(address); });
}

void RPCService::stop() {
  if (active_) {
    active_ = false;
    rpc_thread_.join();
  }
}

int RPCService::send_message(const vector<vector<uint8_t>> msg,
                             const int zmq_connection_id) {
  if (!active_) {
    log_error(LOGGING_TAG_RPC,
              "Cannot send message to inactive RPCService instance",
              "Dropping Message");
    return -1;
  }
  int id = message_id_;
  message_id_ += 1;
  long current_time_micros =
      std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count();
  RPCRequest request(zmq_connection_id, id, std::move(msg),
                     current_time_micros);
  request_queue_->enqueue(request);
  return id;
}

vector<RPCResponse> RPCService::try_get_responses(const int max_num_responses) {
  std::vector<RPCResponse> vec(max_num_responses);
  size_t num_dequeued =
      response_queue_->try_dequeue_bulk(vec.begin(), vec.size());
  vec.resize(num_dequeued);
  return vec;
}

void RPCService::manage_service(const string address) {
  // Map from container id to unique routing id for zeromq
  // Note that zeromq socket id is a byte vector
  log_info_formatted(LOGGING_TAG_RPC, "RPC thread started at address: ",
                     address);
  boost::bimap<int, vector<uint8_t>> connections;
  // Initializes a map to associate the ZMQ connection IDs
  // of connected containers with their metadata, including
  // model id and replica id

  std::unordered_map<std::vector<uint8_t>, ConnectedContainerInfo,
                     std::function<size_t(const std::vector<uint8_t> &vec)>>
      connections_containers_map(INITIAL_REPLICA_ID_SIZE, hash_vector<uint8_t>);
  context_t context = context_t(1);
  socket_t socket = socket_t(context, ZMQ_ROUTER);
  socket.bind(address);
  // Indicate that we will poll our zmq service socket for new inbound messages
  zmq::pollitem_t items[] = {{socket, 0, ZMQ_POLLIN, 0}};
  int zmq_connection_id = 0;
  auto redis_connection = std::make_shared<redox::Redox>();
  Config &conf = get_config();
  while (!redis_connection->connect(conf.get_redis_address(),
                                    conf.get_redis_port())) {
    log_error(LOGGING_TAG_RPC, "RPCService failed to connect to Redis",
              "Retrying in 1 second...");
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  while (active_) {
    // Set poll timeout based on whether there are outgoing messages to
    // send. If there are messages to send, don't let the poll block at all.
    // If there no messages to send, let the poll block for 1 ms.
    int poll_timeout = 0;
    if (request_queue_->size_approx() == 0) {
      poll_timeout = 1;
    }
    zmq_poll(items, 1, poll_timeout);
    if (items[0].revents & ZMQ_POLLIN) {
      // TODO: Balance message sending and receiving fairly
      // Note: We only receive one message per event loop iteration
      log_info(LOGGING_TAG_RPC, "Found message to receive");
      receive_message(socket, connections, connections_containers_map,
                      zmq_connection_id, redis_connection);
    }
    auto current_time = std::chrono::system_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(
            current_time - last_activity_check_time_)
            .count() > CONTAINER_EXISTENCE_CHECK_FREQUENCY_MILLS) {
      check_container_activity(connections_containers_map);
      last_activity_check_time_ = current_time;
    }
    // Note: We send all queued messages per event loop iteration
    send_messages(socket, connections);
  }
  shutdown_service(socket);
}

void RPCService::check_container_activity(
    std::unordered_map<std::vector<uint8_t>, ConnectedContainerInfo,
                       std::function<size_t(const std::vector<uint8_t> &vec)>>
        &connections_containers_map) {
  std::chrono::system_clock::time_point current_time =
      std::chrono::system_clock::now();

  auto it = connections_containers_map.begin();
  while (it != connections_containers_map.end()) {
    auto &container_info = it->second;
    if (std::chrono::duration_cast<std::chrono::milliseconds>(
            current_time - std::get<2>(container_info))
            .count() > CONTAINER_ACTIVITY_TIMEOUT_MILLS) {
      /** if the amount of time that has elapsed between the current time and
      the time of last
      receiving from the container is greater than the threshold, then we want
      to
      call the inactive_container_callback_ */

      VersionedModelId vm = std::get<0>(container_info);
      int replica_id = std::get<1>(container_info);
      GarbageCollectionThreadPool::submit_job(inactive_container_callback_, vm,
                                              replica_id);

      log_info(LOGGING_TAG_RPC, "lost contact with a container");
      connections_containers_map.erase(it);
    }
    ++it;
  }
}

void RPCService::shutdown_service(socket_t &socket) {
  size_t buf_size = 32;
  std::vector<char> buf(buf_size);
  socket.getsockopt(ZMQ_LAST_ENDPOINT, (void *)buf.data(), &buf_size);
  std::string last_endpoint = std::string(buf.begin(), buf.end());
  socket.unbind(last_endpoint);
  socket.close();
}

void RPCService::send_messages(
    socket_t &socket, boost::bimap<int, vector<uint8_t>> &connections) {
  int queue_size = request_queue_->size_approx();
  std::vector<RPCRequest> requests(queue_size);
  size_t num_requests =
      request_queue_->try_dequeue_bulk(requests.begin(), queue_size);
  for (size_t i = 0; i < num_requests; i++) {
    RPCRequest &request = requests[i];
    long current_time_micros =
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();
    msg_queueing_hist_->insert(current_time_micros - std::get<3>(request));
    boost::bimap<int, vector<uint8_t>>::left_const_iterator connection =
        connections.left.find(std::get<0>(request));
    if (connection == connections.left.end()) {
      // Error handling
      log_error_formatted(LOGGING_TAG_CLIPPER,
                          "Attempted to send message to unknown container: ",
                          std::get<0>(request));
      continue;
    }

    message_t type_message(sizeof(int));
    static_cast<int *>(type_message.data())[0] =
        static_cast<int>(MessageType::ContainerContent);
    message_t id_message(sizeof(int));
    memcpy(id_message.data(), &std::get<1>(request), sizeof(int));
    vector<uint8_t> routing_identity = connection->second;

    socket.send(routing_identity.data(), routing_identity.size(), ZMQ_SNDMORE);
    socket.send("", 0, ZMQ_SNDMORE);
    socket.send(type_message, ZMQ_SNDMORE);
    socket.send(id_message, ZMQ_SNDMORE);
    int cur_msg_num = 0;
    // subtract 1 because we start counting at 0
    int last_msg_num = std::get<2>(request).size() - 1;
    for (const std::vector<uint8_t> &m : std::get<2>(request)) {
      // send the sndmore flag unless we are on the last message part
      if (cur_msg_num < last_msg_num) {
        socket.send((uint8_t *)m.data(), m.size(), ZMQ_SNDMORE);
      } else {
        socket.send((uint8_t *)m.data(), m.size(), 0);
      }
      cur_msg_num += 1;
    }
  }
}

void RPCService::receive_message(
    socket_t &socket, boost::bimap<int, vector<uint8_t>> &connections,
    std::unordered_map<std::vector<uint8_t>, ConnectedContainerInfo,
                       std::function<size_t(const std::vector<uint8_t> &vec)>>
        &connections_containers_map,
    int &zmq_connection_id, std::shared_ptr<redox::Redox> redis_connection) {
  message_t msg_routing_identity;
  message_t msg_delimiter;
  message_t msg_type;
  socket.recv(&msg_routing_identity, 0);
  socket.recv(&msg_delimiter, 0);
  socket.recv(&msg_type, 0);

  const vector<uint8_t> connection_id(
      (uint8_t *)msg_routing_identity.data(),
      (uint8_t *)msg_routing_identity.data() + msg_routing_identity.size());
  MessageType type =
      static_cast<MessageType>(static_cast<int *>(msg_type.data())[0]);

  boost::bimap<int, vector<uint8_t>>::right_const_iterator connection =
      connections.right.find(connection_id);
  bool new_connection = (connection == connections.right.end());

  if (!new_connection) {
    // If message corresponds to a container that has previously
    // identified itself by sending metadata, we should document
    // that we've received a message from it recently
    document_receive_time(connections_containers_map, connection_id);
  }

  switch (type) {
    case MessageType::NewContainer: {
      message_t model_name;
      message_t model_version;
      message_t model_input_type;
      socket.recv(&model_name, 0);
      socket.recv(&model_version, 0);
      socket.recv(&model_input_type, 0);
      if (new_connection) {
        // We have a new connection with container metadata, process it
        // accordingly
        connections.insert(boost::bimap<int, vector<uint8_t>>::value_type(
            zmq_connection_id, connection_id));
        log_info(LOGGING_TAG_RPC, "New container connected");
        std::string name(static_cast<char *>(model_name.data()),
                         model_name.size());
        std::string version(static_cast<char *>(model_version.data()),
                            model_version.size());
        std::string input_type_str(static_cast<char *>(model_input_type.data()),
                                   model_input_type.size());

        InputType input_type =
            static_cast<InputType>(std::stoi(input_type_str));

        VersionedModelId model = VersionedModelId(name, version);
        log_info(LOGGING_TAG_RPC, "Container added");

        // Note that if the map does not have an entry for this model,
        // a new entry will be created with the default value (0).
        // This use of operator[] avoids the boilerplate of having to
        // check if the key is present in the map.
        int cur_replica_id = replica_ids_[model];
        replica_ids_[model] = cur_replica_id + 1;
        redis::add_container(*redis_connection, model, cur_replica_id,
                             zmq_connection_id, input_type);

        auto connected_time = std::chrono::system_clock::now();

        connections_containers_map.emplace(
            connection_id,
            std::make_tuple(model, cur_replica_id, connected_time));

        TaskExecutionThreadPool::create_queue(model, cur_replica_id);
        zmq_connection_id += 1;
      }
    } break;
    case MessageType::ContainerContent: {
      // This message is a response to a container query
      message_t msg_id;
      message_t msg_content;
      socket.recv(&msg_id, 0);
      socket.recv(&msg_content, 0);
      if (!new_connection) {
        int id = static_cast<int *>(msg_id.data())[0];
        vector<uint8_t> content(
            (uint8_t *)msg_content.data(),
            (uint8_t *)msg_content.data() + msg_content.size());
        RPCResponse response(id, content);

        auto container_info_entry =
            connections_containers_map.find(connection_id);
        if (container_info_entry == connections_containers_map.end()) {
          throw std::runtime_error(
              "Failed to find container that was previously registered via "
              "RPC");
        }
        ConnectedContainerInfo &container_info = container_info_entry->second;

        VersionedModelId vm = std::get<0>(container_info);
        int replica_id = std::get<1>(container_info);
        TaskExecutionThreadPool::submit_job(vm, replica_id,
                                            new_response_callback_, response);
        TaskExecutionThreadPool::submit_job(
            vm, replica_id, container_ready_callback_, vm, replica_id);

        response_queue_->enqueue(response);
      }
    } break;
    case MessageType::Heartbeat: {
      send_heartbeat_response(socket, connection_id, new_connection);
    } break;
  }
}

void RPCService::document_receive_time(
    std::unordered_map<std::vector<uint8_t>, ConnectedContainerInfo,
                       std::function<size_t(const std::vector<uint8_t> &vec)>>
        &connections_containers_map,
    const vector<uint8_t> connection_id) {
  auto container_info_entry = connections_containers_map.find(connection_id);
  if (container_info_entry == connections_containers_map.end()) {
    throw std::runtime_error(
        "Documenting receive time for an unregistered container");
  }
  std::chrono::time_point<std::chrono::system_clock> &last_contacted_time =
      std::get<2>(container_info_entry->second);
  last_contacted_time = std::chrono::system_clock::now();
}

void RPCService::send_heartbeat_response(socket_t &socket,
                                         const vector<uint8_t> &connection_id,
                                         bool request_container_metadata) {
  message_t type_message(sizeof(int));
  message_t heartbeat_type_message(sizeof(int));
  static_cast<int *>(type_message.data())[0] =
      static_cast<int>(MessageType::Heartbeat);
  static_cast<int *>(heartbeat_type_message.data())[0] = static_cast<int>(
      request_container_metadata ? HeartbeatType::RequestContainerMetadata
                                 : HeartbeatType::KeepAlive);
  socket.send(connection_id.data(), connection_id.size(), ZMQ_SNDMORE);
  socket.send("", 0, ZMQ_SNDMORE);
  socket.send(type_message, ZMQ_SNDMORE);
  socket.send(heartbeat_type_message);
}

}  // namespace rpc

}  // namespace clipper
