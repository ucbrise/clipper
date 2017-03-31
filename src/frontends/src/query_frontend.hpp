#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <clipper/config.hpp>
#include <clipper/constants.hpp>
#include <clipper/datatypes.hpp>
#include <clipper/json_util.hpp>
#include <clipper/logging.hpp>
#include <clipper/metrics.hpp>
#include <clipper/query_processor.hpp>
#include <clipper/redis.hpp>

#include <server_http.hpp>

using clipper::Response;
using clipper::FeedbackAck;
using clipper::VersionedModelId;
using clipper::InputType;
using clipper::Input;
using clipper::Output;
using clipper::Query;
using clipper::Feedback;
using clipper::FeedbackQuery;
using clipper::json::json_parse_error;
using clipper::json::json_semantic_error;
using HttpServer = SimpleWeb::Server<SimpleWeb::HTTP>;

namespace query_frontend {

const std::string LOGGING_TAG_QUERY_FRONTEND = "QUERYFRONTEND";
const std::string GET_METRICS = "^/metrics$";

const char* RESPONSE_KEY_QUERY_ID = "query_id";
const char* RESPONSE_KEY_OUTPUT = "output";
const char* RESPONSE_KEY_USED_DEFAULT = "default";

const std::string PREDICTION_JSON_SCHEMA = R"(
  {
   "uid" := string,
   "input" := [double] | [int] | [string] | [byte] | [float],
  }
)";

const std::string UPDATE_JSON_SCHEMA = R"(
  {
   "uid" := string,
   "input" := [double] | [int] | [string] | [byte] | [float],
   "label" := double
  }
)";

void respond_http(std::string content, std::string message,
                  std::shared_ptr<HttpServer::Response> response) {
  *response << "HTTP/1.1 " << message
            << "\r\nContent-Length: " << content.length() << "\r\n\r\n"
            << content << "\n";
}

/* Generate a user-facing error message containing the exception
 * content and the expected JSON schema. */
std::string json_error_msg(const std::string& exception_msg,
                           const std::string& expected_schema) {
  std::stringstream ss;
  ss << "Error parsing JSON: " << exception_msg << ". "
     << "Expected JSON schema: " << expected_schema;
  return ss.str();
}

template <class QP>
class RequestHandler {
 public:
  RequestHandler(std::string address, int portno, int num_threads)
      : server_(address, portno, num_threads), query_processor_() {
    clipper::Config& conf = clipper::get_config();
    while (!redis_connection_.connect(conf.get_redis_address(),
                                      conf.get_redis_port())) {
      clipper::log_error(LOGGING_TAG_QUERY_FRONTEND,
                         "Query frontend failed to connect to Redis",
                         "Retrying in 1 second...");
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    while (!redis_subscriber_.connect(conf.get_redis_address(),
                                      conf.get_redis_port())) {
      clipper::log_error(LOGGING_TAG_QUERY_FRONTEND,
                         "Query frontend subscriber failed to connect to Redis",
                         "Retrying in 1 second...");
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    server_.add_endpoint(GET_METRICS, "GET",
                         [](std::shared_ptr<HttpServer::Response> response,
                            std::shared_ptr<HttpServer::Request> /*request*/) {
                           clipper::metrics::MetricsRegistry& registry =
                               clipper::metrics::MetricsRegistry::get_metrics();
                           std::string metrics_report =
                               registry.report_metrics();
                           clipper::log_info(LOGGING_TAG_QUERY_FRONTEND,
                                             "METRICS", metrics_report);
                           respond_http(metrics_report, "200 OK", response);
                         });

    clipper::redis::subscribe_to_application_changes(
        redis_subscriber_,
        [this](const std::string& key, const std::string& event_type) {
          clipper::log_info_formatted(
              LOGGING_TAG_QUERY_FRONTEND,
              "APPLICATION EVENT DETECTED. Key: {}, event_type: {}", key,
              event_type);
          if (event_type == "hset") {
            std::string name = key;
            clipper::log_info_formatted(LOGGING_TAG_QUERY_FRONTEND,
                                        "New application detected: {}", key);
            auto app_info =
                clipper::redis::get_application_by_key(redis_connection_, key);
            std::vector<std::string> candidate_model_names =
                clipper::redis::str_to_model_names(
                    app_info["candidate_model_names"]);
            InputType input_type =
                clipper::parse_input_type(app_info["input_type"]);
            std::string policy = app_info["policy"];
            std::string default_output = app_info["default_output"];
            int latency_slo_micros = std::stoi(app_info["latency_slo_micros"]);
            add_application(name, candidate_models, input_type, policy,
                            default_output, latency_slo_micros);
          }
        });
  }

  ~RequestHandler() {
    redis_connection_.disconnect();
    redis_subscriber_.disconnect();
  }

  void add_application(std::string name, std::vector<std::string> models,
                       InputType input_type, std::string policy,
                       std::string default_output, long latency_slo_micros) {
    // TODO: QueryProcessor should handle this. We need to decide how the
    // default output fits into the generic selection policy API. Do all
    // selection policies have a default output?
    //
    // Initialize selection state for this application
    if (policy == clipper::DefaultOutputSelectionPolicy::get_name()) {
      clipper::DefaultOutputSelectionPolicy p;
      clipper::Output parsed_default_output(std::stod(default_output), {});
      auto init_state = p.init_state(parsed_default_output);
      clipper::StateKey state_key{name, clipper::DEFAULT_USER_ID, 0};
      query_processor_.get_state_table()->put(state_key,
                                              p.serialize(init_state));
    }

    auto predict_fn = [this, name, input_type, policy, latency_slo_micros,
                       models](std::shared_ptr<HttpServer::Response> response,
                               std::shared_ptr<HttpServer::Request> request) {
      try {
        std::vector<VersionedModelId> versioned_models;
        {
          std::unique_lock<std::mutex> l(current_model_versions_mutex_);
          for (auto m : models) {
            auto version = current_model_versions_.find(m);
            if (version != current_model_versions_.end()) {
              versioned_models.emplace_back(m, version->second);
            }
          }
        }
        auto prediction = decode_and_handle_predict(
            request->content.string(), name, versioned_models, policy,
            latency_slo_micros, input_type);
        prediction.then([response](boost::future<Response> f) {
          Response r = f.get();
          std::stringstream ss;
          ss << "qid:" << r.query_id_ << ", predict:" << r.output_.y_hat_;
          rapidjson::Document json_response;
          json_response.SetObject();
          clipper::json::add_long(json_response, RESPONSE_KEY_QUERY_ID, r.query_id_);
          clipper::json::add_double(json_response, RESPONSE_KEY_OUTPUT, r.output_.y_hat_);
          clipper::json::add_bool(json_response, RESPONSE_KEY_USED_DEFAULT, r.output_is_default_);

          //std::string content = ss.str();
          std::string content = clipper::json::to_json_string(json_response);
          respond_http(content, "200 OK", response);
        });
      } catch (const json_parse_error& e) {
        std::string error_msg =
            json_error_msg(e.what(), PREDICTION_JSON_SCHEMA);
        respond_http(error_msg, "400 Bad Request", response);
      } catch (const json_semantic_error& e) {
        std::string error_msg =
            json_error_msg(e.what(), PREDICTION_JSON_SCHEMA);
        respond_http(error_msg, "400 Bad Request", response);
      } catch (const std::invalid_argument& e) {
        respond_http(e.what(), "400 Bad Request", response);
      }
    };
    std::string predict_endpoint = "^/" + name + "/predict$";
    server_.add_endpoint(predict_endpoint, "POST", predict_fn);

    auto update_fn = [this, name, input_type, policy, models](
        std::shared_ptr<HttpServer::Response> response,
        std::shared_ptr<HttpServer::Request> request) {
      try {
        std::vector<VersionedModelId> versioned_models;
        {
          std::unique_lock<std::mutex> l(current_model_versions_mutex_);
          for (auto m : models) {
            auto version = current_model_versions_.find(m);
            if (version != current_model_versions_.end()) {
              versioned_models.emplace_back(m, version->second);
            }
          }
        }
        auto update =
            decode_and_handle_update(request->content.string(), name,
                                     versioned_models, policy, input_type);
        update.then([response](boost::future<FeedbackAck> f) {
          FeedbackAck ack = f.get();
          std::stringstream ss;
          ss << "Feedback received? " << ack;
          std::string content = ss.str();
          respond_http(content, "200 OK", response);
        });
      } catch (const json_parse_error& e) {
        std::string error_msg = json_error_msg(e.what(), UPDATE_JSON_SCHEMA);
        respond_http(error_msg, "400 Bad Request", response);
      } catch (const json_semantic_error& e) {
        std::string error_msg = json_error_msg(e.what(), UPDATE_JSON_SCHEMA);
        respond_http(error_msg, "400 Bad Request", response);
      } catch (const std::invalid_argument& e) {
        respond_http(e.what(), "400 Bad Request", response);
      }
    };
    std::string update_endpoint = "^/" + name + "/update$";
    server_.add_endpoint(update_endpoint, "POST", update_fn);
  }

  /*
   * JSON format for prediction query request:
   * {
   *  "uid" := string,
   *  "input" := [double] | [int] | [string] | [byte] | [float],
   * }
   */
  boost::future<Response> decode_and_handle_predict(
      std::string json_content, std::string name,
      std::vector<VersionedModelId> models, std::string policy,
      long latency_slo_micros, InputType input_type) {
    rapidjson::Document d;
    clipper::json::parse_json(json_content, d);
    long uid = clipper::json::get_long(d, "uid");
    std::shared_ptr<Input> input = clipper::json::parse_input(input_type, d);
    auto prediction = query_processor_.predict(
        Query{name, uid, input, latency_slo_micros, policy, models});
    return prediction;
  }

  /*
   * JSON format for feedback query request:
   * {
   *  "uid" := string,
   *  "input" := [double] | [int] | [string] | [byte] | [float],
   *  "label" := double
   * }
   */
  boost::future<FeedbackAck> decode_and_handle_update(
      std::string json_content, std::string name,
      std::vector<VersionedModelId> models, std::string policy,
      InputType input_type) {
    rapidjson::Document d;
    clipper::json::parse_json(json_content, d);
    long uid = clipper::json::get_long(d, "uid");
    std::shared_ptr<Input> input = clipper::json::parse_input(input_type, d);
    double y_hat = clipper::json::get_double(d, "label");
    auto update = query_processor_.update(
        FeedbackQuery{name, uid, {Feedback(input, y_hat)}, policy, models});
    return update;
  }

  void start_listening() { server_.start(); }

  /**
   * Returns the number of applications that have been registered
   * with Clipper. This is equivalent to the number of /predict,/update
   * REST endpoint pairs that have been registered with the server.
   * We don't count the /metrics endpoint as it does not serve predictions.
   */
  size_t num_applications() {
    // Subtract one to account for the /metrics endpoint
    size_t count = server_.num_endpoints() - 1;
    assert(count % 2 == 0);
    return count / 2;
  }

 private:
  HttpServer server_;
  QP query_processor_;
  redox::Redox redis_connection_;
  redox::Subscriber redis_subscriber_;
  std::mutex current_model_versions_mutex_;
  std::unordered_map<std::string, int> current_model_versions_;
};

}  // namespace query_frontend
