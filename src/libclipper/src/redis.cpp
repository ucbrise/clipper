#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <clipper/constants.hpp>
#include <clipper/redis.hpp>
#include <redox.hpp>

using redox::Command;
using redox::Redox;
using redox::Subscriber;
using std::string;
using std::vector;
using std::unordered_map;

namespace clipper {
namespace redis {

std::unordered_map<string, string> parse_redis_map(
    const std::vector<string>& redis_data) {
  std::unordered_map<string, string> parsed_map;
  for (auto m = redis_data.begin(); m != redis_data.end(); ++m) {
    auto key = *m;
    m += 1;
    auto value = *m;
    std::cout << "\t" << key << ": " << value << std::endl;
    parsed_map[key] = value;
  }
  return parsed_map;
}

std::string gen_model_replica_key(const VersionedModelId& key,
                                  int model_replica_id) {
  std::stringstream ss;
  ss << key.first;
  ss << ":";
  ss << key.second;
  ss << ":";
  ss << model_replica_id;
  return ss.str();
}

std::string gen_versioned_model_key(const VersionedModelId& key) {
  std::stringstream ss;
  ss << key.first;
  ss << ":";
  ss << key.second;
  return ss.str();
}

string labels_to_str(const vector<string>& labels) {
  std::ostringstream ss;
  for (auto l = labels.begin(); l != labels.end() - 1; ++l) {
    ss << *l << ITEM_DELIMITER;
  }
  // don't forget to save the last label
  ss << *(labels.end() - 1);
  return ss.str();
}

// String parsing taken from http://stackoverflow.com/a/14267455/814642
vector<string> str_to_labels(const string& label_str) {
  auto start = 0;
  auto end = label_str.find(ITEM_DELIMITER);
  vector<string> labels;

  while (end != string::npos) {
    labels.push_back(label_str.substr(start, end - start));
    start = end + ITEM_DELIMITER.length();
    end = label_str.find(ITEM_DELIMITER, start);
  }
  // don't forget to parse the last label
  labels.push_back(label_str.substr(start, end - start));
  return labels;
}

std::string models_to_str(const std::vector<VersionedModelId>& models) {
  std::ostringstream ss;
  for (auto m = models.begin(); m != models.end() - 1; ++m) {
    ss << m->first << ITEM_PART_CONCATENATOR << m->second << ITEM_DELIMITER;
  }
  // don't forget to save the last label
  ss << (models.end() - 1)->first << ITEM_PART_CONCATENATOR
     << (models.end() - 1)->second;
  std::cout << "models_to_str result: " << ss.str() << std::endl;
  return ss.str();
}

std::vector<VersionedModelId> str_to_models(const std::string& model_str) {
  auto start = 0;
  auto end = model_str.find(ITEM_DELIMITER);
  vector<VersionedModelId> models;

  while (end != string::npos) {
    auto split =
        start +
        model_str.substr(start, end - start).find(ITEM_PART_CONCATENATOR);
    std::string model_name = model_str.substr(start, split - start);
    std::string model_version_str =
        model_str.substr(split + 1, end - split - 1);
    int version = std::stoi(model_version_str);
    models.push_back(std::make_pair(model_name, version));
    start = end + ITEM_DELIMITER.length();
    end = model_str.find(ITEM_DELIMITER, start);
  }

  // don't forget to parse the last model
  auto split =
      start + model_str.substr(start, end - start).find(ITEM_PART_CONCATENATOR);
  std::string model_name = model_str.substr(start, split - start);
  std::string model_version_str = model_str.substr(split + 1, end - split - 1);
  int version = std::stoi(model_version_str);
  models.push_back(std::make_pair(model_name, version));

  return models;
}

bool add_model(Redox& redis, const VersionedModelId& model_id,
               const InputType& input_type, const std::string& output_type,
               const vector<string>& labels, const std::string& container_name,
               const std::string& model_data_path) {
  if (send_cmd_no_reply<string>(
          redis, {"SELECT", std::to_string(REDIS_MODEL_DB_NUM)})) {
    std::string model_id_key = gen_versioned_model_key(model_id);
    // clang-format off
    const vector<string> cmd_vec{
      "HMSET",            model_id_key,
      "model_name",       model_id.first,
      "model_version",    std::to_string(model_id.second),
      "load",             std::to_string(0.0),
      "input_type",       get_readable_input_type(input_type),
      "output_type",      output_type,
      "labels",           labels_to_str(labels),
      "container_name",   container_name,
      "model_data_path",  model_data_path};
    // clang-format on
    return send_cmd_no_reply<string>(redis, cmd_vec);
  } else {
    return false;
  }
}

bool delete_model(Redox& redis, const VersionedModelId& model_id) {
  if (send_cmd_no_reply<string>(
          redis, {"SELECT", std::to_string(REDIS_MODEL_DB_NUM)})) {
    std::string model_id_key = gen_versioned_model_key(model_id);
    return send_cmd_no_reply<int>(redis, {"DEL", model_id_key});
  } else {
    return false;
  }
}

unordered_map<string, string> get_model(Redox& redis,
                                        const VersionedModelId& model_id) {
  if (send_cmd_no_reply<string>(
          redis, {"SELECT", std::to_string(REDIS_MODEL_DB_NUM)})) {
    std::string model_id_key = gen_versioned_model_key(model_id);

    std::vector<std::string> model_data;
    auto result =
        send_cmd_with_reply<vector<string>>(redis, {"HGETALL", model_id_key});
    if (result) {
      model_data = *result;
    }
    return parse_redis_map(model_data);
  } else {
    return unordered_map<string, string>{};
  }
}

unordered_map<string, string> get_model_by_key(Redox& redis,
                                               const std::string& key) {
  if (send_cmd_no_reply<string>(
          redis, {"SELECT", std::to_string(REDIS_MODEL_DB_NUM)})) {
    std::vector<std::string> model_data;
    auto result = send_cmd_with_reply<vector<string>>(redis, {"HGETALL", key});
    if (result) {
      model_data = *result;
    }
    return parse_redis_map(model_data);
  } else {
    return unordered_map<string, string>{};
  }
}

bool add_container(Redox& redis, const VersionedModelId& model_id,
                   const int model_replica_id, const int zmq_connection_id,
                   const InputType& input_type) {
  if (send_cmd_no_reply<string>(
          redis, {"SELECT", std::to_string(REDIS_CONTAINER_DB_NUM)})) {
    std::string replica_key = gen_model_replica_key(model_id, model_replica_id);
    std::string model_id_key = gen_versioned_model_key(model_id);
    const vector<string> cmd_vec{
        "HMSET", replica_key, "model_id", model_id_key, "model_name",
        model_id.first, "model_version", std::to_string(model_id.second),
        "model_replica_id", std::to_string(model_replica_id),
        "zmq_connection_id", std::to_string(zmq_connection_id), "batch_size",
        std::to_string(1), "input_type", get_readable_input_type(input_type)

    };
    return send_cmd_no_reply<string>(redis, cmd_vec);
  } else {
    return false;
  }
}

bool delete_container(Redox& redis, const VersionedModelId& model_id,
                      const int model_replica_id) {
  if (send_cmd_no_reply<string>(
          redis, {"SELECT", std::to_string(REDIS_CONTAINER_DB_NUM)})) {
    std::string replica_key = gen_model_replica_key(model_id, model_replica_id);
    return send_cmd_no_reply<int>(redis, {"DEL", replica_key});
  } else {
    return false;
  }
}

unordered_map<string, string> get_container(Redox& redis,
                                            const VersionedModelId& model_id,
                                            const int model_replica_id) {
  if (send_cmd_no_reply<string>(
          redis, {"SELECT", std::to_string(REDIS_CONTAINER_DB_NUM)})) {
    std::string replica_key = gen_model_replica_key(model_id, model_replica_id);
    std::vector<std::string> container_data;
    auto result =
        send_cmd_with_reply<vector<string>>(redis, {"HGETALL", replica_key});
    if (result) {
      container_data = *result;
    }
    return parse_redis_map(container_data);
  } else {
    return unordered_map<string, string>{};
  }
}

unordered_map<string, string> get_container_by_key(Redox& redis,
                                                   const std::string& key) {
  if (send_cmd_no_reply<string>(
          redis, {"SELECT", std::to_string(REDIS_CONTAINER_DB_NUM)})) {
    std::vector<std::string> container_data;
    auto result = send_cmd_with_reply<vector<string>>(redis, {"HGETALL", key});
    if (result) {
      container_data = *result;
    }
    return parse_redis_map(container_data);
  } else {
    return unordered_map<string, string>{};
  }
}

bool add_application(redox::Redox& redis, const std::string& appname,
                     const std::vector<VersionedModelId>& models,
                     const InputType& input_type,
                     const std::string& output_type, const std::string& policy,
                     const long latency_slo_micros) {
  if (send_cmd_no_reply<string>(
          redis, {"SELECT", std::to_string(REDIS_APPLICATION_DB_NUM)})) {
    const vector<string> cmd_vec{"HMSET",
                                 appname,
                                 "candidate_models",
                                 models_to_str(models),
                                 "input_type",
                                 get_readable_input_type(input_type),
                                 "output_type",
                                 output_type,
                                 "policy",
                                 policy,
                                 "latency_slo_micros",
                                 std::to_string(latency_slo_micros)};
    return send_cmd_no_reply<string>(redis, cmd_vec);
  } else {
    return false;
  }
}

bool delete_application(redox::Redox& redis, const std::string& appname) {
  if (send_cmd_no_reply<string>(
          redis, {"SELECT", std::to_string(REDIS_APPLICATION_DB_NUM)})) {
    return send_cmd_no_reply<int>(redis, {"DEL", appname});
  } else {
    return false;
  }
}

std::unordered_map<std::string, std::string> get_application(
    redox::Redox& redis, const std::string& appname) {
  if (send_cmd_no_reply<string>(
          redis, {"SELECT", std::to_string(REDIS_APPLICATION_DB_NUM)})) {
    std::vector<std::string> container_data;
    auto result =
        send_cmd_with_reply<vector<string>>(redis, {"HGETALL", appname});
    if (result) {
      container_data = *result;
    }

    return parse_redis_map(container_data);
  } else {
    return unordered_map<string, string>{};
  }
}

std::unordered_map<std::string, std::string> get_application_by_key(
    redox::Redox& redis, const std::string& key) {
  // Applications just use their appname as a key.
  // We keep the get_*_by_key() to preserve the symmetry of the
  // API.
  return get_application(redis, key);
}

void subscribe_to_keyspace_changes(
    int db, Subscriber& subscriber,
    std::function<void(const std::string&, const std::string&)> callback) {
  std::ostringstream subscription;
  subscription << "__keyspace@" << std::to_string(db) << "__:*";
  std::string sub_str = subscription.str();
  std::cout << "SUBSCRIPTION STRING: " << sub_str << std::endl;
  subscriber.psubscribe(
      sub_str, [callback](const std::string& topic, const std::string& msg) {
        size_t split_idx = topic.find_first_of(":");
        std::string key = topic.substr(split_idx + 1);
        std::cout << "MESSAGE: " << msg << std::endl;
        callback(key, msg);
      });
}

void subscribe_to_model_changes(
    Subscriber& subscriber,
    std::function<void(const std::string&, const std::string&)> callback) {
  subscribe_to_keyspace_changes(REDIS_MODEL_DB_NUM, subscriber,
                                std::move(callback));
}

void subscribe_to_container_changes(
    Subscriber& subscriber,
    std::function<void(const std::string&, const std::string&)> callback) {
  subscribe_to_keyspace_changes(REDIS_CONTAINER_DB_NUM, subscriber,
                                std::move(callback));
}

void subscribe_to_application_changes(
    redox::Subscriber& subscriber,
    std::function<void(const std::string&, const std::string&)> callback) {
  subscribe_to_keyspace_changes(REDIS_APPLICATION_DB_NUM, subscriber,
                                std::move(callback));
}

}  // namespace redis
}  // namespace clipper
