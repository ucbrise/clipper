#include <iostream>
#include <memory>
#include <string>

#include <cassert>
#include <chrono>
#include <thread>
#include <utility>
#include <vector>

#include <boost/optional.hpp>
#include <redox.hpp>

#include <grpcpp/grpcpp.h>

#include "management.grpc.pb.h"

#include <clipper/config.hpp>
#include <clipper/datatypes.hpp>
#include <clipper/exceptions.hpp>
#include <clipper/json_util.hpp>
#include <clipper/logging.hpp>
#include <clipper/persistent_state.hpp>
#include <clipper/redis.hpp>
#include <clipper/selection_policies.hpp>
#include <clipper/util.hpp>

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using management::ApplicationInfo;
using management::DAGInfo;
using management::HelloReply;
using management::HelloRequest;
using management::LinkInfo;
using management::ManagementServer;
using management::ModelContainerInfo;
using management::ModelInfo;
using management::ProxyContainerInfo;
using management::Response;
using management::RuntimeDAGInfo;

// Logic and data behind the server's behavior.
class GreeterServiceImpl final : public Greeter::Service {
 public:
  GreeterServiceImpl(redox::Redox* redis, redox::Subscriber* subscriber)
      : redis_connection_(redis), redis_subscriber_(subscriber) {}

  Status SayHello(ServerContext* context, const HelloRequest* request,
                  HelloReply* reply) override {
    std::string prefix("Hello ");
    reply->set_message(prefix + request->name());
    return Status::OK;
  }

  Status AddModel(ServerContext* context, const ModelInfo* request,
                  Response* reply) override {
    std::string prefix("Hello ");

    std::string modelname(request->modelname());

    reply->set_status(prefix + request->name());
    return Status::OK;
  }

 private:
  redox::Redox* redis_connection_;
  redox::Subscriber* redis_subscriber_;
};

std::string add_model(const std::string& json) {
  rapidjson::Document d;
  parse_json(json, d);
  std::string model_name = get_string(d, "model_name");
  std::string model_version = get_string(d, "model_version");
  VersionedModelId model_id = VersionedModelId(model_name, model_version);

  std::vector<std::string> labels = get_string_array(d, "labels");
  std::string input_type_raw = get_string(d, "input_type");
  InputType input_type = clipper::parse_input_type(input_type_raw);
  std::string container_name = get_string(d, "container_name");
  std::string model_data_path = get_string(d, "model_data_path");
  int batch_size = get_int(d, "batch_size");

  // The batch_size should be either positive or DEFAULT_BATCH_SIZE
  if (batch_size <= 0 && batch_size != DEFAULT_BATCH_SIZE) {
    std::stringstream ss;
    ss << "The batch size must be positive or DEFAULT_BATCH_SIZE, which is "
          "-1";
    throw clipper::ManagementOperationError(ss.str());
  }
  // Validate strings that will be grouped before supplying to redis
  validate_group_str_for_redis(model_name, "model name");
  validate_group_str_for_redis(model_id.get_id(), "model version");
  for (auto label : labels) {
    validate_group_str_for_redis(label, "label");
  }

  // check if this version of the model has already been deployed
  std::unordered_map<std::string, std::string> existing_model_data =
      clipper::redis::get_model(redis_connection_, model_id);

  if (!existing_model_data.empty()) {
    std::stringstream ss;
    ss << "model with name "
       << "'" << model_name << "'"
       << " and version "
       << "'" << model_version << "'"
       << " already exists";
    throw clipper::ManagementOperationError(ss.str());
  }

  check_updated_model_consistent_with_app_links(
      VersionedModelId(model_name, model_version),
      boost::optional<InputType>(input_type));

  if (clipper::redis::add_model(redis_connection_, model_id, input_type, labels,
                                container_name, model_data_path, batch_size)) {
    attempt_model_version_update(model_id.get_name(), model_id.get_id());
    std::stringstream ss;
    ss << "Successfully added model with name "
       << "'" << model_name << "'"
       << " and input type "
       << "'" << clipper::get_readable_input_type(input_type) << "'";
    return ss.str();
  }
  std::stringstream ss;
  ss << "Error adding model " << model_id.get_name() << ":" << model_id.get_id()
     << " to Redis";
  throw clipper::ManagementOperationError(ss.str());
}
