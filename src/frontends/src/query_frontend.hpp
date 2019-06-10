#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <boost/exception_ptr.hpp>

#include <folly/futures/Future.h>

#include <clipper/config.hpp>
#include <clipper/constants.hpp>
#include <clipper/datatypes.hpp>
#include <clipper/exceptions.hpp>
#include <clipper/json_util.hpp>
#include <clipper/logging.hpp>
#include <clipper/metrics.hpp>
#include <clipper/query_processor.hpp>
#include <clipper/redis.hpp>

#include <server_http.hpp>

using clipper::Feedback;
using clipper::FeedbackAck;
using clipper::FeedbackQuery;
using clipper::InputType;
using clipper::Output;
using clipper::PredictionData;
using clipper::Query;
using clipper::Response;
using clipper::VersionedModelId;
using clipper::json::json_parse_error;
using clipper::json::json_semantic_error;
using clipper::redis::labels_to_str;
using HttpServer = SimpleWeb::Server<SimpleWeb::HTTP>;

namespace query_frontend {

const std::string LOGGING_TAG_QUERY_FRONTEND = "QUERYFRONTEND";
const std::string GET_METRICS = "^/metrics$";

const char* PREDICTION_RESPONSE_KEY_QUERY_ID = "query_id";
const char* PREDICTION_RESPONSE_KEY_OUTPUT = "output";
const char* PREDICTION_RESPONSE_KEY_USED_DEFAULT = "default";
const char* PREDICTION_RESPONSE_KEY_DEFAULT_EXPLANATION = "default_explanation";
const char* PREDICTION_ERROR_RESPONSE_KEY_ERROR = "error";
const char* PREDICTION_ERROR_RESPONSE_KEY_CAUSE = "cause";
const char* PREDICTION_BATCH_INDICATOR_KEY = "batch_predictions";

const std::string PREDICTION_ERROR_NAME_JSON = "Json error";
const std::string PREDICTION_ERROR_NAME_QUERY_PROCESSING =
    "Query processing error";

const std::string PREDICTION_JSON_SCHEMA = R"(
  {
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
  *response << "HTTP/1.1 " << message << "\r\nContent-Type: application/json"
            << "\r\nContent-Length: " << content.length() << "\r\n\r\n"
            << content;
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

/* Create error class for if an invalid model version id is passed in to
 * json query. */
class version_id_error : public std::runtime_error {
 public:
  version_id_error(const std::string msg)
      : std::runtime_error(msg), msg_(msg) {}

  const char* what() const noexcept { return msg_.c_str(); }

 private:
  const std::string msg_;
};

class AppMetrics {
 public:
  explicit AppMetrics(std::string app_name)
      : app_name_(app_name),
        latency_(
            clipper::metrics::MetricsRegistry::get_metrics().create_histogram(
                "app:" + app_name + ":prediction_latency", "microseconds",
                4096)),
        throughput_(
            clipper::metrics::MetricsRegistry::get_metrics().create_meter(
                "app:" + app_name + ":prediction_throughput")),
        num_predictions_(
            clipper::metrics::MetricsRegistry::get_metrics().create_counter(
                "app:" + app_name + ":num_predictions")),
        default_pred_ratio_(
            clipper::metrics::MetricsRegistry::get_metrics()
                .create_ratio_counter("app:" + app_name +
                                      ":default_prediction_ratio")) {}
  ~AppMetrics() = default;

  AppMetrics(const AppMetrics&) = default;

  AppMetrics& operator=(const AppMetrics&) = default;

  AppMetrics(AppMetrics&&) = default;
  AppMetrics& operator=(AppMetrics&&) = default;

  std::string app_name_;
  std::shared_ptr<clipper::metrics::Histogram> latency_;
  std::shared_ptr<clipper::metrics::Meter> throughput_;
  std::shared_ptr<clipper::metrics::Counter> num_predictions_;
  std::shared_ptr<clipper::metrics::RatioCounter> default_pred_ratio_;
};

template <class QP>
class RequestHandler {
 public:
  RequestHandler(std::string address, int portno,
                 int thread_pool_size = clipper::DEFAULT_THREAD_POOL_SIZE,
                 int timeout_request = clipper::DEFAULT_TIMEOUT_REQUEST,
                 int timeout_content = clipper::DEFAULT_TIMEOUT_CONTENT)
      : server_(address, portno, thread_pool_size, timeout_request,
                timeout_content),
        query_processor_() {
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

                          // There's no point to log here because the data is collected in prometheus
                          //  clipper::log_info(LOGGING_TAG_QUERY_FRONTEND,
                          //                    "METRICS", metrics_report);
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
            InputType input_type =
                clipper::parse_input_type(app_info["input_type"]);
            std::string policy = app_info["policy"];
            std::string default_output = app_info["default_output"];
            int latency_slo_micros = std::stoi(app_info["latency_slo_micros"]);
            add_application(name, input_type, policy, default_output,
                            latency_slo_micros);
          } else if (event_type == "hdel") {
            std::string name = key;
            delete_application(name);
          }
        });

    clipper::redis::subscribe_to_model_link_changes(
        redis_subscriber_,
        [this](const std::string& key, const std::string& event_type) {
          std::string app_name = key;
          clipper::log_info_formatted(
              LOGGING_TAG_QUERY_FRONTEND,
              "APP LINKS EVENT DETECTED. App name: {}, event_type: {}",
              app_name, event_type);
          if (event_type == "sadd") {
            clipper::log_info_formatted(LOGGING_TAG_QUERY_FRONTEND,
                                        "New model link detected for app: {}",
                                        app_name);
            auto linked_model_names =
                clipper::redis::get_linked_models(redis_connection_, app_name);
            set_linked_models_for_app(app_name, linked_model_names);

          } else if (event_type == "srem") {
            clipper::log_info_formatted(LOGGING_TAG_QUERY_FRONTEND,
                                        "Model link removal detected for app: {}", app_name);
            auto linked_model_names =
                clipper::redis::get_linked_models(redis_connection_, app_name);
            set_linked_models_for_app(app_name, linked_model_names);
          }
        });

    clipper::redis::subscribe_to_model_version_changes(
        redis_subscriber_,
        [this](const std::string& key, const std::string& event_type) {
          clipper::log_info_formatted(
              LOGGING_TAG_QUERY_FRONTEND,
              "MODEL VERSION CHANGE DETECTED. Key: {}, event_type: {}", key,
              event_type);
          if (event_type == "set") {
            std::string model_name = key;
            boost::optional<std::string> new_version =
                clipper::redis::get_current_model_version(redis_connection_,
                                                          key);
            if (new_version) {
              std::unique_lock<std::mutex> l(current_model_versions_mutex_);
              current_model_versions_[key] = *new_version;
            } else {
              clipper::log_error_formatted(
                  LOGGING_TAG_QUERY_FRONTEND,
                  "Model version change for model {} was invalid.", key);
            }
          }
        });

    // Read from Redis configuration tables and update models/applications.
    // (1) Iterate through applications and set up predict/update endpoints.
    std::vector<std::string> app_names =
        clipper::redis::get_all_application_names(redis_connection_);
    for (std::string app_name : app_names) {
      auto app_info =
          clipper::redis::get_application_by_key(redis_connection_, app_name);

      auto linked_model_names =
          clipper::redis::get_linked_models(redis_connection_, app_name);
      set_linked_models_for_app(app_name, linked_model_names);

      InputType input_type = clipper::parse_input_type(app_info["input_type"]);
      std::string policy = app_info["policy"];
      std::string default_output = app_info["default_output"];
      int latency_slo_micros = std::stoi(app_info["latency_slo_micros"]);

      add_application(app_name, input_type, policy, default_output,
                      latency_slo_micros);
    }
    if (app_names.size() > 0) {
      clipper::log_info_formatted(
          LOGGING_TAG_QUERY_FRONTEND,
          "Found {} existing applications registered in Clipper: {}.",
          app_names.size(), labels_to_str(app_names));
    }
    // (2) Update current_model_versions_ with (model, version) pairs.
    std::vector<std::string> model_names =
        clipper::redis::get_all_model_names(redis_connection_);
    // Record human-readable model names for logging
    std::vector<std::string> model_names_with_version;
    for (std::string model_name : model_names) {
      auto model_version = clipper::redis::get_current_model_version(
          redis_connection_, model_name);
      if (model_version) {
        std::unique_lock<std::mutex> l(current_model_versions_mutex_);
        current_model_versions_[model_name] = *model_version;
        model_names_with_version.push_back(model_name + "@" + *model_version);
      } else {
        clipper::log_error_formatted(
            LOGGING_TAG_QUERY_FRONTEND,
            "Found model {} with missing current version.", model_name);
        throw std::runtime_error("Invalid model version");
      }
    }
    if (model_names.size() > 0) {
      clipper::log_info_formatted(LOGGING_TAG_QUERY_FRONTEND,
                                  "Found {} models deployed to Clipper: {}.",
                                  model_names.size(),
                                  labels_to_str(model_names_with_version));
    }
  }

  ~RequestHandler() {
    redis_connection_.disconnect();
    redis_subscriber_.disconnect();
  }

  void set_linked_models_for_app(std::string name,
                                 std::vector<std::string> models) {
    std::unique_lock<std::mutex> l(linked_models_for_apps_mutex_);
    linked_models_for_apps_[name] = models;
  }

  std::vector<std::string> get_linked_models_for_app(std::string name) {
    std::unique_lock<std::mutex> l(linked_models_for_apps_mutex_);
    return linked_models_for_apps_[name];
  }

  void add_application(std::string name, InputType input_type,
                       std::string policy, std::string default_output,
                       long latency_slo_micros) {
    // TODO: QueryProcessor should handle this. We need to decide how the
    // default output fits into the generic selection policy API. Do all
    // selection policies have a default output?

    // Initialize selection state for this application
    if (policy == clipper::DefaultOutputSelectionPolicy::get_name()) {
      clipper::DefaultOutputSelectionPolicy p;
      clipper::Output parsed_default_output(default_output, {});
      auto init_state = p.init_state(parsed_default_output);
      clipper::StateKey state_key{name, clipper::DEFAULT_USER_ID, 0};
      query_processor_.get_state_table()->put(state_key,
                                              p.serialize(init_state));
    }

    AppMetrics app_metrics(name);

    /*
     * JSON format for prediction query request:
     * {
     *  "input" := [double] | [int] | [string] | [byte] | [float]
     *  "input_batch" := [[double] | [int] | [byte] | [float] | string]
     *  "version" := string (optional)
     * }
     */

    auto predict_fn = [this, name, input_type, policy, latency_slo_micros,
                       app_metrics](
        std::shared_ptr<HttpServer::Response> response,
        std::shared_ptr<HttpServer::Request> request) {
      try {
        folly::Future<std::vector<folly::Try<Response>>> predictions =
            decode_and_handle_predict(request->content.string(), name, policy,
                                      latency_slo_micros, input_type);

        std::move(predictions)
            .thenValue([response,
                   app_metrics](std::vector<folly::Try<Response>> tries) {
              std::vector<std::string> all_content;
              for (auto t : tries) {
                try {
                  Response r = t.value();
                  if (r.output_is_default_) {
                    app_metrics.default_pred_ratio_->increment(1, 1);
                  } else {
                    app_metrics.default_pred_ratio_->increment(0, 1);
                  }
                  app_metrics.latency_->insert(r.duration_micros_);
                  app_metrics.num_predictions_->increment(1);
                  app_metrics.throughput_->mark(1);

                  std::string content = get_prediction_response_content(r);
                  all_content.push_back(content);
                } catch (const std::exception& e) {
                  clipper::log_error(LOGGING_TAG_QUERY_FRONTEND,
                                     "Returned response before all predictions "
                                     "in batch were processed");
                }
              }

              std::string final_content =
                  get_batch_prediction_response_content(all_content);
              respond_http(final_content, "200 OK", response);
            })
            .thenError(folly::tag_t<std::exception>{}, [response](const std::exception& e) {
              clipper::log_error_formatted(clipper::LOGGING_TAG_CLIPPER,
                                           "Unexpected error: {}", e.what());
              respond_http("An unexpected error occurred!",
                           "500 Internal Server Error", response);
            });
      } catch (const json_parse_error& e) {
        std::string error_msg =
            json_error_msg(e.what(), PREDICTION_JSON_SCHEMA);
        std::string json_error_response = get_prediction_error_response_content(
            PREDICTION_ERROR_NAME_JSON, error_msg);
        respond_http(json_error_response, "400 Bad Request", response);
      } catch (const json_semantic_error& e) {
        std::string error_msg =
            json_error_msg(e.what(), PREDICTION_JSON_SCHEMA);
        std::string json_error_response = get_prediction_error_response_content(
            PREDICTION_ERROR_NAME_JSON, error_msg);
        respond_http(json_error_response, "400 Bad Request", response);
      } catch (const std::invalid_argument& e) {
        // This invalid argument exception is most likely the propagation of an
        // exception thrown
        // when Rapidjson attempts to parse an invalid json schema
        std::string json_error_response = get_prediction_error_response_content(
            PREDICTION_ERROR_NAME_JSON, e.what());
        respond_http(json_error_response, "400 Bad Request", response);
      } catch (const clipper::PredictError& e) {
        std::string error_msg = e.what();
        std::string json_error_response = get_prediction_error_response_content(
            PREDICTION_ERROR_NAME_QUERY_PROCESSING, error_msg);
        respond_http(json_error_response, "400 Bad Request", response);
      } catch (const version_id_error& e) {
        std::string error_msg = e.what();
        std::string json_error_response = get_prediction_error_response_content(
            PREDICTION_ERROR_NAME_QUERY_PROCESSING, error_msg);
        respond_http(json_error_response, "400 Bad Request", response);
      }
    };
    std::string predict_endpoint = "^/" + name + "/predict/?$";
    server_.add_endpoint(predict_endpoint, "POST", predict_fn);

    auto update_fn = [this, name, input_type, policy](
        std::shared_ptr<HttpServer::Response> response,
        std::shared_ptr<HttpServer::Request> request) {
      try {
        std::vector<std::string> models = get_linked_models_for_app(name);
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
        folly::Future<FeedbackAck> update =
            decode_and_handle_update(request->content.string(), name,
                                     versioned_models, policy, input_type);
        std::move(update).thenValue([response](FeedbackAck ack) {
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
    std::string update_endpoint = "^/" + name + "/update/?$";
    server_.add_endpoint(update_endpoint, "POST", update_fn);
  }

  static const std::string parse_output_y_hat(
      std::shared_ptr<PredictionData>& y_hat) {
    SharedPoolPtr<char> str_content = clipper::get_data<char>(y_hat);
    return std::string(str_content.get() + y_hat->start(),
                       str_content.get() + y_hat->start() + y_hat->size());
  }

  void delete_application(std::string name) {
    std::string predict_endpoint = "^/" + name + "/predict/?$";
    server_.delete_endpoint(predict_endpoint, "POST");
    std::string update_endpoint = "^/" + name + "/update/?$";
    server_.delete_endpoint(update_endpoint, "POST");
  }

  /**
   * Obtains the json-formatted http response content for a successful query
   *
   * JSON format for prediction response:
   * {
   *    "query_id" := int,
   *    "output" := float,
   *    "default" := boolean
   *    "default_explanation" := string (optional)
   * }
   */
  static const std::string get_prediction_response_content(
      Response& query_response) {
    rapidjson::Document json_response;
    json_response.SetObject();
    clipper::json::add_long(json_response, PREDICTION_RESPONSE_KEY_QUERY_ID,
                            query_response.query_id_);
    rapidjson::Document json_y_hat;
    std::string y_hat_str = parse_output_y_hat(query_response.output_.y_hat_);
    try {
      // Attempt to parse the string output as JSON
      // and, if possible, nest it in object form within the
      // query response
      clipper::json::parse_json(y_hat_str, json_y_hat);
      clipper::json::add_object(json_response, PREDICTION_RESPONSE_KEY_OUTPUT,
                                json_y_hat);
    } catch (const clipper::json::json_parse_error& e) {
      // If the string output is not JSON-formatted, include
      // it as a JSON-safe string value in the query response
      clipper::json::add_string(json_response, PREDICTION_RESPONSE_KEY_OUTPUT,
                                y_hat_str);
    }
    clipper::json::add_bool(json_response, PREDICTION_RESPONSE_KEY_USED_DEFAULT,
                            query_response.output_is_default_);
    if (query_response.output_is_default_ &&
        query_response.default_explanation_) {
      clipper::json::add_string(json_response,
                                PREDICTION_RESPONSE_KEY_DEFAULT_EXPLANATION,
                                query_response.default_explanation_.get());
    }
    std::string content = clipper::json::to_json_string(json_response);
    return content;
  }

  /**
   * Obtains the json-formatted http response content for a successful query
   *
   * JSON format for prediction response:
   * {
   *   "batch_predictions" :
   *     {
   *        "query_id" := int,
   *        "output" := float,
   *        "default" := boolean
   *        "default_explanation" := string (optional)
   *     }
   * }
   */
  static const std::string get_batch_prediction_response_content(
      std::vector<std::string> query_responses) {
    if (query_responses.size() == 1) {
      return query_responses[0];
    }

    rapidjson::Document json_response;
    json_response.SetObject();
    try {
      clipper::json::add_json_array(
          json_response, PREDICTION_BATCH_INDICATOR_KEY, query_responses);
    } catch (const clipper::json::json_parse_error& e) {
      clipper::json::add_string_array(
          json_response, PREDICTION_BATCH_INDICATOR_KEY, query_responses);
    }

    std::string content = clipper::json::to_json_string(json_response);
    return content;
  }

  /**
   * Obtains the json-formatted http response content for a query
   * that could not be completed due to an error
   *
   * JSON format for error prediction response:
   * {
   *    "error" := string,
   *    "cause" := string
   * }
   */
  static const std::string get_prediction_error_response_content(
      const std::string error_name, const std::string error_msg) {
    rapidjson::Document error_response;
    error_response.SetObject();
    clipper::json::add_string(error_response,
                              PREDICTION_ERROR_RESPONSE_KEY_ERROR, error_name);
    clipper::json::add_string(error_response,
                              PREDICTION_ERROR_RESPONSE_KEY_CAUSE, error_msg);
    return clipper::json::to_json_string(error_response);
  }

  folly::Future<std::vector<folly::Try<Response>>> decode_and_handle_predict(
      std::string content, std::string name, std::string policy,
      long latency_slo_micros, InputType input_type) {
    rapidjson::Document d;
    clipper::json::parse_json(content, d);
    std::vector<VersionedModelId> versioned_models = {};
    std::vector<std::string> linked_models = get_linked_models_for_app(name);
    if (d.HasMember("version")) {
      std::string requested_version = clipper::json::get_string(d, "version");
      if (linked_models.size() > 1) {
        std::stringstream ss;
        ss << "Too many models linked to application " << name;
        throw clipper::PredictError(ss.str());
      }
      // NOTE: This implementation assumes that there is only model per
      // application, and therefore the version provided by the user
      // unambiguously applies to that model.
      for (auto m : linked_models) {
        std::vector<std::string> registered_versions =
            clipper::redis::get_model_versions(redis_connection_, m);
        for (auto v : registered_versions) {
          if (v == requested_version) {
            versioned_models = {
                clipper::VersionedModelId(m, requested_version)};
            break;
          }
        }
        // There should be at most one linked model to this application, so
        // we break here.
        break;
      }

      if (versioned_models.empty()) {
        std::string model_name = linked_models[0];
        std::stringstream ss;
        ss << "Requested version: " << requested_version
           << " does not exist for model: " << model_name;
        throw version_id_error(ss.str());
      }

    } else {
      {
        std::unique_lock<std::mutex> l(current_model_versions_mutex_);
        for (auto m : linked_models) {
          auto version = current_model_versions_.find(m);
          if (version != current_model_versions_.end()) {
            versioned_models.emplace_back(m, version->second);
          }
        }
      }
    }

    long uid = 0;

    std::vector<std::shared_ptr<PredictionData>> input_batch =
        clipper::json::parse_inputs(input_type, d);
    std::vector<folly::Future<Response>> predictions;
    for (auto input : input_batch) {
      auto prediction = query_processor_.predict(Query{
          name, uid, input, latency_slo_micros, policy, versioned_models});
      predictions.push_back(std::move(prediction));
    }
    return folly::collectAll(predictions);
  }

  /*
   * JSON format for feedback query request:
   * {
   *  "uid" := string,
   *  "input" := [double] | [int] | [string] | [byte] | [float],
   *  "label" := double
   * }
   */
  folly::Future<FeedbackAck> decode_and_handle_update(
      std::string json_content, std::string name,
      std::vector<VersionedModelId> models, std::string policy,
      InputType input_type) {
    rapidjson::Document d;
    clipper::json::parse_json(json_content, d);
    long uid = clipper::json::get_long(d, "uid");
    std::shared_ptr<PredictionData> input =
        clipper::json::parse_single_input(input_type, d);
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

  /**
   * Returns a copy of the map containing current model names and versions.
   */
  std::unordered_map<std::string, std::string> get_current_model_versions() {
    return current_model_versions_;
  }

 private:
  HttpServer server_;
  QP query_processor_;
  redox::Redox redis_connection_;
  redox::Subscriber redis_subscriber_;
  std::mutex current_model_versions_mutex_;
  std::unordered_map<std::string, std::string> current_model_versions_;

  std::mutex linked_models_for_apps_mutex_;
  std::unordered_map<std::string, std::vector<std::string>>
      linked_models_for_apps_;
};

}  // namespace query_frontend
