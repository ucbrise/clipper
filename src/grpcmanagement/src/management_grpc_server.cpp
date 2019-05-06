#include <iostream>
#include <memory>
#include <string>
#include <thread>


#include <grpcpp/grpcpp.h>

#include <cxxopts.hpp>

#include <clipper/config.hpp>
#include <clipper/constants.hpp>



#include "management.grpc.pb.h"


using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using management::HelloRequest;
using management::HelloReply;
using management::Greeter;


#include "management_grpc_server.hpp"


void RunServer(int port) {

  clipper::Config& conf = clipper::get_config();

  redox::Redox redis_connection_;
  redox::Subscriber redis_subscriber_;
  //clipper::StateDB state_db_;

  while (!redis_connection_.connect(conf.get_redis_address(),
                                    conf.get_redis_port())) {
    clipper::log_error(LOGGING_TAG_MANAGEMENT_FRONTEND,
                        "Management frontend failed to connect to Redis",
                        "Retrying in 1 second...");
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  while (!redis_subscriber_.connect(conf.get_redis_address(),
                                    conf.get_redis_port())) {
    clipper::log_error(
        LOGGING_TAG_MANAGEMENT_FRONTEND,
        "Management frontend subscriber failed to connect to Redis",
        "Retrying in 1 second...");
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  std::string server_address("0.0.0.0:50051");
  GreeterServiceImpl service(&redis_connection_, &redis_subscriber_);

  ServerBuilder builder;
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  // Register "service" as the instance through which we'll communicate with
  // clients. In this case it corresponds to an *synchronous* service.
  builder.RegisterService(&service);
  // Finally assemble the server.
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;

  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  server->Wait();
}

int main(int argc, char** argv) {

  cxxopts::Options options("management_frontend",
                           "Clipper management interface");

  // clang-format off
  options.add_options()
    ("redis_ip", "Redis address", cxxopts::value<std::string>()->default_value("localhost"))
    ("redis_port", "Redis port", cxxopts::value<int>()->default_value("33333"));
  // clang-format on
  options.parse(argc, argv);    

  clipper::Config& conf = clipper::get_config();
  conf.set_redis_address(options["redis_ip"].as<std::string>());
  conf.set_redis_port(options["redis_port"].as<int>());
  conf.ready();                  

  RunServer(33334);

  return 0;
}



