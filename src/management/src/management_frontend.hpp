#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <boost/thread.hpp>
#define BOOST_SPIRIT_THREADSAFE
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <redox.hpp>
#include <server_http.hpp>

#include <clipper/constants.hpp>
#include <clipper/datatypes.hpp>
#include <clipper/util.hpp>

using namespace boost::property_tree;
using HttpServer = SimpleWeb::Server<SimpleWeb::HTTP>;
using clipper::VersionedModelId;
using clipper::InputType;

// Redis stubs:
// TODO: Delete once redis-pubsub is committed
namespace clipper {
namespace redis {

void insert_application(std::string /*name*/,
                        std::vector<VersionedModelId> /*models*/,
                        InputType /*input_type*/, std::string /*output_type*/,
                        std::string /*policy*/, long /*latency*/) {}

void insert_model(VersionedModelId >, InputType, std::string,
                  std::vector<std::string>) {}

}  // namespace redis
}  // namespace clipper

namespace management {

const std::string ADMIN_PATH = "^/admin$";
const std::string ADD_APPLICATION = ADMIN_PATH + "/add_app$";
const std::string ADD_MODEL = ADMIN_PATH + "/add_model$";

// const std::string ADD_CONTAINER = ADMIN_PATH + "/add_container$";
const std::string GET_METRICS = ADMIN_PATH + "/metrics$";
const std::string GET_POLICY_STATE = ADMIN_PATH + "/policy_state$";
const std::string GET_APPLICATIONS = ADMIN_PATH + "/applications$";

template <typename T>
std::vector<T> as_vector(ptree const& pt, ptree::key_type const& key) {
  std::vector<T> r;
  for (auto& item : pt.get_child(key)) {
    r.push_back(item.second.get_value<T>());
  }
  return r;
}

/**
 * @param models_node A ptree with a child object named `key` that
 * contains an array of VersionedModelId objects
 * objects.
 */
std::vector<VersionedModelId> parse_candidate_models(
    const ptree& models_node, ptree::key_type const& key) {
  std::vector<VersionedModelId> models;
  for (auto vm_json : models_node.get_child(key)) {
    VersionedModelId cur_vm =
        std::make_pair(vm_json.second.get<std::string>("model_name"),
                       vm_json.second.get<int>("model_version"));
    models.push_back(cur_vm);
  }
  return models;
}

void respond_http(std::string content, std::string message,
                  std::shared_ptr<HttpServer::Response> response) {
  *response << "HTTP/1.1 " << message
            << "\r\nContent-Length: " << content.length() << "\r\n\r\n"
            << content << "\n";
}

class RequestHandler {
 public:
  RequestHandler(int portno, int num_threads) : server_(portno, num_threads) {
    redis_connection_.connect(clipper::REDIS_IP, clipper::REDIS_PORT);
    server_.add_endpoint(
        ADD_APPLICATION, "POST",
        [this](std::shared_ptr<HttpServer::Response> response,
               std::shared_ptr<HttpServer::Request> request) {
          try {
            std::string result = add_application(response, request);
            respond_http(result, "200 OK", response);
          } catch (const ptree_error& e) {
            respond_http(e.what(), "400 Bad Request", response);
          } catch (const std::invalid_argument& e) {
            respond_http(e.what(), "400 Bad Request", response);
          }
        });
  }

  /**
   * Creates an endpoint that listens for requests to add new prediction
   * applications to Clipper.
   *
   * JSON format:
   * {
   *  "name" := string,
   *  "candidate_models" := [{"model_name" := string, "model_version" := int}],
   *  "input_type" := "integers" | "bytes" | "floats" | "doubles" | "strings",
   *  "output_type" := "double" | "int",
   *  "selection_policy" := string,
   *  "latency_slo_micros" := int
   * }
   */
  std::string add_application(
      std::shared_ptr<HttpServer::Response> /*response*/,
      std::shared_ptr<HttpServer::Request> request) {
    std::string json = request->content.string();
    std::istringstream is(json);
    ptree pt;
    read_json(is, pt);
    std::string app_name = pt.get<std::string>("name");
    std::vector<VersionedModelId> candidate_models =
        parse_candidate_models(pt, "candidate_models");
    InputType input_type =
        clipper::parse_input_type(pt.get<std::string>("input_type"));
    std::string output_type = pt.get<std::string>("output_type");
    if (output_type != "int" || output_type != "double") {
      throw std::invalid_argument(output_type + " is invalid output type");
    }
    std::string selection_policy = pt.get<std::string>("selection_policy");
    int latency_slo_micros = pt.get<int>("latency_slo_micros");
    clipper::redis::insert_application(app_name, candidate_models, input_type,
                                       output_type, selection_policy,
                                       latency_slo_micros);

    return "Success!";
  }

  /**
   * Creates an endpoint that listens for requests to add new models to Clipper.
   *
   * JSON format:
   * {
   *  "model_name" := string,
   *  "model_version" := int,
   *  "labels" := [string]
   *  "input_type" := "integers" | "bytes" | "floats" | "doubles" | "strings",
   *  "output_type" := "double" | "int",
   * }
   */
  std::string add_model(std::shared_ptr<HttpServer::Response> /*response*/,
                        std::shared_ptr<HttpServer::Request> request) {
    std::string json = request->content.string();
    std::istringstream is(json);
    ptree pt;
    read_json(is, pt);

    VersionedModelId model_id = std::make_pair(
        pt.get<std::string>("model_name"), pt.get<int>("model_version"));
    std::vector<std::string> labels as_vector(pt, "labels");
    InputType input_type =
        clipper::parse_input_type(pt.get<std::string>("input_type"));
    std::string output_type = pt.get<std::string>("output_type");
    if (output_type != "int" || output_type != "double") {
      throw std::invalid_argument(output_type + " is invalid output type");
    }
    clipper::redis::insert_model(model_id, input_type, output_type, labels);

    return "Success!";
  }

  // void add_application(std::string name, std::vector<VersionedModelId>
  // models,
  //                      InputType input_type, OutputType output_type,
  //                      std::string policy, long latency) {
  //   auto predict_fn = [this, name, input_type, policy, latency, models](
  //       std::shared_ptr<HttpServer::Response> response,
  //       std::shared_ptr<HttpServer::Request> request) {
  //     try {
  //       auto prediction =
  //           decode_and_handle_predict(request->content.string(), name,
  //           models,
  //                                     policy, latency, input_type);
  //       prediction.then([response](boost::future<Response> f) {
  //         Response r = f.get();
  //         std::stringstream ss;
  //         ss << "qid:" << r.query_id_ << " predict:" << r.output_.y_hat_;
  //         std::string content = ss.str();
  //         respond_http(content, "200 OK", response);
  //       });
  //     } catch (const ptree_error& e) {
  //       respond_http(e.what(), "400 Bad Request", response);
  //     } catch (const std::invalid_argument& e) {
  //       respond_http(e.what(), "400 Bad Request", response);
  //     }
  //   };
  //   std::string predict_endpoint = "^/" + name + "/predict$";
  //   server_.add_endpoint(predict_endpoint, "POST", predict_fn);
  //
  //   auto update_fn = [this, name, input_type, output_type, policy, models](
  //       std::shared_ptr<HttpServer::Response> response,
  //       std::shared_ptr<HttpServer::Request> request) {
  //     try {
  //       auto update =
  //           decode_and_handle_update(request->content.string(), name, models,
  //                                    policy, input_type, output_type);
  //       update.then([response](boost::future<FeedbackAck> f) {
  //         FeedbackAck ack = f.get();
  //         std::stringstream ss;
  //         ss << "Feedback received? " << ack;
  //         std::string content = ss.str();
  //         respond_http(content, "200 OK", response);
  //       });
  //     } catch (const ptree_error& e) {
  //       respond_http(e.what(), "400 Bad Request", response);
  //     } catch (const std::invalid_argument& e) {
  //       respond_http(e.what(), "400 Bad Request", response);
  //     }
  //   };
  //   std::string update_endpoint = "^/" + name + "/update$";
  //   server_.add_endpoint(update_endpoint, "POST", update_fn);
  // }

  boost::future<Response> decode_and_handle_predict(
      std::string json_content, std::string name,
      std::vector<VersionedModelId> models, std::string policy, long latency,
      InputType input_type) {
    std::istringstream is(json_content);
    ptree pt;
    read_json(is, pt);

    long uid = pt.get<long>("uid");
    std::shared_ptr<Input> input = decode_input(input_type, pt);
    auto prediction = query_processor_.predict(
        Query{name, uid, input, latency, policy, models});

    return prediction;
  }

  /* Update JSON format:
   * {"uid": <user id>, "input": <query input>, "label": <query y_hat>,
   *  "model_name": <model name>, "model_version": <model_version>}
   */
  boost::future<FeedbackAck> decode_and_handle_update(
      std::string json_content, std::string name,
      std::vector<VersionedModelId> models, std::string policy,
      InputType input_type, OutputType output_type) {
    std::istringstream is(json_content);
    ptree pt;
    read_json(is, pt);

    long uid = pt.get<long>("uid");
    std::shared_ptr<Input> input = decode_input(input_type, pt);
    Output output = decode_output(output_type, pt);
    auto update = query_processor_.update(FeedbackQuery{
        name, uid, {std::make_pair(input, output)}, policy, models});

    return update;
  }

  void start_listening() {
    HttpServer& s = server_;
    std::thread server_thread([&s]() { s.start(); });

    server_thread.join();
  }

 private:
  HttpServer server_;
  redox::Redox redis_connection_;
};

}  // namespace management
