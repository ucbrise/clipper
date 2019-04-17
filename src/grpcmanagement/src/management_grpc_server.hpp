#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>


#include "management.grpc.pb.h"


using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using management::HelloRequest;
using management::HelloReply;
using management::ModelInfo;
using management::ModelContainerInfo;
using management::ProxyContainerInfo;
using management::DAGInfo;
using management::RuntimeDAGInfo;
using management::ApplicationInfo;
using management::LinkInfo;
using management::Response;
using management::ManagementServer;

// Logic and data behind the server's behavior.
class GreeterServiceImpl final : public Greeter::Service {
  Status SayHello(ServerContext* context, const HelloRequest* request,
                  HelloReply* reply) override  {
    std::string prefix("Hello ");
    reply->set_message(prefix + request->name());
    return Status::OK;
  }

  Status AddModel(ServerContext* context, const ModelInfo* request,
                  Response* reply) override  {
    std::string prefix("Hello ");

    std::string modelname(request->modelname()); 



    reply->set_status(prefix + request->name());
    return Status::OK;
  }
};


