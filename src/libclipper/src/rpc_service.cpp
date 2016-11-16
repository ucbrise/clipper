#include <iostream>
#include <clipper/util.hpp>
#include <clipper/rpc_service.hpp>
#include <boost/thread.hpp>
#include <boost/bimap.hpp>

using zmq::socket_t;
using zmq::message_t;
using zmq::context_t;
using std::shared_ptr;
using std::string;
using std::vector;

namespace clipper {

RPCService::RPCService() :
    request_queue_(std::make_shared<Queue<RPCRequest>>()),
    response_queue_(std::make_shared<Queue<RPCResponse>>()) {
}

RPCService::~RPCService() {
  stop();
}

void RPCService::start(const string ip, const int port) {
  const string address = "tcp://" + ip + ":" + std::to_string(port);
  active_ = true;
  // TODO: Propagate errors from new child thread for handling
  // TODO: Explore bind vs static method call for thread creation
  boost::thread(boost::bind(&RPCService::manage_service,
                            this,
                            address,
                            request_queue_,
                            response_queue_,
                            boost::ref(active_))).detach();
}

void RPCService::stop() {
  active_ = false;
}

int RPCService::send_message(const std::vector<uint8_t> &msg, const int container_id) {
  if(!active_) {
    return -1;
  }
  int id = message_id_++;
  RPCRequest request(container_id, id, msg.data(), msg.size());
  request_queue_->push(request);
  return id;
}

vector<RPCResponse> RPCService::try_get_responses(const int max_num_responses) {
  vector<RPCResponse> responses;
  for(int i = 0; i < max_num_responses; i++) {
    if(auto response = response_queue_->try_pop()) {
      responses.push_back(*response);
    } else {
      break;
    }
  }
  return responses;

}

void RPCService::manage_service(const string address,
                                shared_ptr<Queue<RPCRequest>> request_queue,
                                shared_ptr<Queue<RPCResponse>> response_queue,
                                const bool &active) {
  // Map from container id to unique routing id for zeromq
  // Note that zeromq socket id is a byte vector
  boost::bimap<int, vector<uint8_t>> connections;
  context_t context = context_t(1);
  socket_t socket = socket_t(context, ZMQ_ROUTER);
  socket.bind(address);
  // Indicate that we will poll our zmq service socket for new inbound messages
  zmq::pollitem_t items[] = {
      {socket, 0, ZMQ_POLLIN, 0}
  };
  int container_id = 0;
  while (true) {
    if (active) {
      shutdown_service(address, socket);
      return;
    }
    zmq_poll(items, 1, 0);
    if (items[0].revents & ZMQ_POLLIN) {
      // TODO: Balance message sending and receiving fairly
      // Note: We only receive one message per event loop iteration
      receive_message(socket, response_queue, connections, container_id);
    }
    // Note: We send all queued messages per event loop iteration
    send_messages(socket, request_queue, connections);
  }
}

void RPCService::shutdown_service(const string address, socket_t &socket) {
  socket.disconnect(address);
  socket.close();
}

void RPCService::send_messages(socket_t &socket,
                               shared_ptr<Queue<RPCRequest>> request_queue,
                               boost::bimap<int, vector<uint8_t>> &connections) {
  while (request_queue->size() > 0) {
    RPCRequest request = request_queue->pop();
    boost::bimap<int, vector<uint8_t>>::left_const_iterator connection = connections.left.find(std::get<0>(request));
    if (connection == connections.left.end()) {
      // Error handling
      std::cout << "Attempted to send message to unknown container " << std::get<0>(request) << std::endl;
      continue;
    }
    message_t id_message(sizeof(int));
    memcpy(id_message.data(), &std::get<1>(request), sizeof(int));
    vector<uint8_t> routing_identity = connection->second;
    socket.send(routing_identity.data(), routing_identity.size(), ZMQ_SNDMORE);
    socket.send("", 0, ZMQ_SNDMORE);
    socket.send(id_message, ZMQ_SNDMORE);
    socket.send((uint8_t *) std::get<2>(request), std::get<3>(request), 0);
  }
}

void RPCService::receive_message(socket_t &socket,
                                 shared_ptr<Queue<RPCResponse>> response_queue,
                                 boost::bimap<int, vector<uint8_t>> &connections,
                                 int &container_id) {
  message_t msg_identity;
  message_t msg_delimiter;
  message_t msg_id;
  message_t msg_content;
  socket.recv(&msg_identity, 0);
  socket.recv(&msg_delimiter, 0);
  socket.recv(&msg_id, 0);
  socket.recv(&msg_content, 0);

  vector<uint8_t> connection_id((uint8_t *) msg_identity.data(), (uint8_t *) msg_identity.data() + msg_identity.size());
  boost::bimap<int, vector<uint8_t>>::right_const_iterator connection = connections.right.find(connection_id);
  if (connection == connections.right.end()) {
    // We have a new connection, process it accordingly
    connections.insert(boost::bimap<int, vector<uint8_t>>::value_type(container_id, connection_id));
    container_id++;

  } else {
    int id = ((int *) msg_id.data())[0];
    vector<uint8_t> content((uint8_t *) msg_content.data(), (uint8_t *) msg_content.data() + msg_content.size());
    RPCResponse response(id, content);
    response_queue->push(response);
  }
}

} // namespace clipper