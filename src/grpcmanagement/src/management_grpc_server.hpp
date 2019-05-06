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
using management::ModelContainerInfo;
using management::ModelInfo;
using management::ProxyContainerInfo;
using management::Response;
using management::RuntimeDAGInfo;

using management::ManagementServer;

// Logic and data behind the server's behavior.
class ManagementServerServiceImpl final : public ManagementServer::Service {
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
    // std::string model_name();
    // std::string model_version();
    VersionedModelId model_id =
        VersionedModelId(request->modelname(), request->modelversion());

    InputType input_type = clipper::parse_input_type(request->inputtype());
    OutputType output_type = clipper::parse_input_type(request->outputtype());

    // Validate strings that will be grouped before supplying to redis
    // validate_group_str_for_redis(model_name, "model name");
    // validate_group_str_for_redis(model_id.get_id(), "model version");

    // for (auto label : labels) {
    //   validate_group_str_for_redis(label, "label");
    // }

    // check if this version of the model has already been deployed
    // std::unordered_map<std::string, std::string> existing_model_data =
    //     clipper::redis::get_model(redis_connection_, model_id);

    // if (!existing_model_data.empty()) {
    //   std::stringstream ss;
    //   ss << "model with name "
    //      << "'" << model_name << "'"
    //      << " and version "
    //      << "'" << model_version << "'"
    //      << " already exists";
    //   throw clipper::ManagementOperationError(ss.str());
    // }

    // check_updated_model_consistent_with_app_links(
    // VersionedModelId(model_name, model_version),
    // boost::optional<InputType>(input_type));

    if (clipper::redis::add_model(redis_connection_, model_id, input_type,
                                  output_type, request->stateful(),
                                  request->image())) {
      // attempt_model_version_update(model_id.get_name(), model_id.get_id());
      std::stringstream ss;
      ss << "Successfully added model with name "
         << "'" << model_name << "'"
         << " and input type "
         << "'" << clipper::get_readable_input_type(input_type) << "'";

      reply->set_status(ss.str());
      return Status::OK;
    }
    std::stringstream ss;
    ss << "Error adding model " << model_id.get_name() << ":"
       << model_id.get_id() << " to Redis";
    // throw clipper::ManagementOperationError(ss.str());

    reply->set_status(ss.str());
    return Status::ABORTED;
  }  // end AddModel

  Status AddModelContainer(ServerContext* context,
                           const ModelContainerInfo* request,
                           Response* reply) override {
    // std::string model_name();
    // std::string model_version();
    VersionedModelId model_id =
        VersionedModelId(request->modelname(), request->modelversion());

    // Validate strings that will be grouped before supplying to redis
    // validate_group_str_for_redis(model_name, "model name");
    // validate_group_str_for_redis(model_id.get_id(), "model version");

    if (clipper::redis::add_model_container(
            redis_connection_, model_id, request->ip(), request->host(),
            request->port(), request->containerid(), request->replicaid())) {
      // attempt_model_version_update(model_id.get_name(), model_id.get_id());
      std::stringstream ss;
      ss << "Successfully added container for model_name "
         << "'" << model_name << "(" << request->containerid() << ")'"
         << "with replica_id"
         << "'" << request->replicaid() << "'";

      reply->set_status(ss.str());
      return Status::OK;
    }
    std::stringstream ss;
    ss << "Error adding container for " << model_id.get_name() << ":"
       << model_id.get_id() << " to Redis";
    // throw clipper::ManagementOperationError(ss.str());

    reply->set_status(ss.str());
    return Status::ABORTED;
  }  // end AddModelContainer

  Status AddProxyContainer(ServerContext* context,
                           const ProxyContainerInfo* request,
                           Response* reply) override {
    // std::string model_name();
    // std::string model_version();
    VersionedModelId model_id =
        VersionedModelId(request->modelname(), request->modelversion());

    if (clipper::redis::add_proxy_container(
            redis_connection_, request->proxyname(), model_id, request->ip(),
            request->host(), request->port(), request->containerid())) {
      // attempt_model_version_update(model_id.get_name(), model_id.get_id());
      std::stringstream ss;
      ss << "Successfully added proxy for model_name "
         << "'" << model_name << "(" << request->containerid() << ")'"
         << "with proxy...";

      reply->set_status(ss.str());
      return Status::OK;
    }
    std::stringstream ss;
    ss << "Error adding proxy for " << model_id.get_name() << ":"
       << model_id.get_id() << " to Redis";
    // throw clipper::ManagementOperationError(ss.str());

    reply->set_status(ss.str());
    return Status::ABORTED;
  }  // end AddProxyContainer

  Status AddDAG(ServerContext* context, const DAGInfo* request,
                Response* reply) override {
    VersionedModelId dag_id =
        VersionedModelId(request->name(), request->version());

    if (clipper::redis::add_dag(redis_connection_, request->file(), dag_id,
                                request->format())) {
      // attempt_model_version_update(model_id.get_name(), model_id.get_id());
      std::stringstream ss;
      ss << "Successfully added dag for dag_name "
         << "'" << request->name() << ":" << request->version() << "...";

      reply->set_status(ss.str());
      return Status::OK;
    }
    std::stringstream ss;
    ss << "Error adding dag for " << request->name() << ":"
       << request->version() << " to Redis";
    // throw clipper::ManagementOperationError(ss.str());

    reply->set_status(ss.str());
    return Status::ABORTED;
  }  // end AddDAG

  Status AddRuntimeDAG(ServerContext* context, const RuntimeDAGInfo* request,
                       Response* reply) override {
    VersionedModelId dag_id =
        VersionedModelId(request->name(), request->version());
    if (clipper::redis::add_runtime_dag(redis_connection_, request->file(),
                                        request->id(), dag_id,
                                        request->format())) {
      // attempt_model_version_update(model_id.get_name(), model_id.get_id());
      std::stringstream ss;
      ss << "Successfully added runtime dag for dag_name "
         << "'" << request->name() << ":" << request->version() << "...";

      reply->set_status(ss.str());
      return Status::OK;
    }
    std::stringstream ss;
    ss << "Error adding runtime dag for " << request->name() << ":"
       << request->version() << " to Redis";
    // throw clipper::ManagementOperationError(ss.str());

    reply->set_status(ss.str());
    return Status::ABORTED;
  }  // end AddRuntimeDAG

  /**
   * Attempts to update the version of model with name `model_name` to
   * `new_model_version`.
   */
  // void attempt_model_version_update(const string& model_name,
  //                                   const string& new_model_version) {
  //   if (!clipper::redis::set_current_model_version(
  //           redis_connection_, model_name, new_model_version)) {
  //     std::stringstream ss;
  //     ss << "Version "
  //        << "'" << new_model_version << "'"
  //        << " does not exist for model with name"
  //        << "'" << model_name << "'";
  //     throw clipper::ManagementOperationError(ss.str());
  //   }
  // }

  // void validate_group_str_for_redis(const std::string& value,
  //                                   const char* label) {
  //   if (clipper::redis::contains_prohibited_chars_for_group(value)) {
  //     std::stringstream ss;

  //     ss << "Invalid " << label << " supplied: " << value << ".";
  //     ss << " Contains one of: ";

  //     // Generate string representing list of invalid characters
  //     std::string prohibited_str;
  //     for (size_t i = 0; i != prohibited_group_strings.size() - 1; ++i) {
  //       prohibited_str = *(prohibited_group_strings.begin() + i);
  //       ss << "'" << prohibited_str << "', ";
  //     }
  //     // Add final element of `prohibited_group_strings`
  //     ss << "'" << *(prohibited_group_strings.end() - 1) << "'";

  //     throw clipper::ManagementOperationError(ss.str());
  //   }

 private:
  redox::Redox* redis_connection_;
  redox::Subscriber* redis_subscriber_;
};  // end class
