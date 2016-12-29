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

const std::string LABEL_DELIMITER = ",";

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

vector<string> send_cmd_vec_reply(Redox& redis, vector<string> cmd_vec) {
  vector<string> v;
  redox::Command<vector<string>>& cmd =
      redis.commandSync<vector<string>>(cmd_vec);
  if (!cmd.ok()) {
    std::cerr << "Error with command \"" << redis.vecToStr(cmd_vec)
              << "\": " << cmd.lastError() << std::endl;
  } else {
    std::cout << "Successfully issued command \"" << redis.vecToStr(cmd_vec)
              << "\": " << std::endl;
    v = cmd.reply();
  }
  cmd.free();
  return v;
}

string labels_to_str(const vector<string>& labels) {
  std::ostringstream ss;
  for (auto l = labels.begin(); l != labels.end() - 1; ++l) {
    ss << *l << LABEL_DELIMITER;
  }
  // don't forget to save the last label
  ss << *(labels.end() - 1);
  return ss.str();
}

// String parsing taken from http://stackoverflow.com/a/14267455/814642
vector<string> str_to_labels(const string& label_str) {
  auto start = 0;
  auto end = label_str.find(LABEL_DELIMITER);
  vector<string> labels;

  while (end != string::npos) {
    labels.push_back(label_str.substr(start, end - start));
    start = end + LABEL_DELIMITER.length();
    end = label_str.find(LABEL_DELIMITER, start);
  }
  // don't forget to parse the last label
  labels.push_back(label_str.substr(start, end - start));
  return labels;
}

bool insert_model(Redox& redis, const VersionedModelId& model_id,
                  const vector<string>& labels) {
  if (send_cmd_no_reply<string>(
          redis, {"SELECT", std::to_string(REDIS_MODEL_DB_NUM)})) {
    std::string model_id_key = gen_versioned_model_key(model_id);
    vector<string> cmd_vec{"HMSET",         model_id_key,
                           "model_name",    model_id.first,
                           "model_version", std::to_string(model_id.second),
                           "load",          std::to_string(0.0),
                           "labels",        labels_to_str(labels)};
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
    auto model_data = send_cmd_vec_reply(redis, {"HGETALL", model_id_key});
    return parse_redis_map(model_data);
  } else {
    return unordered_map<string, string>{};
  }
}

unordered_map<string, string> get_model_by_key(Redox& redis,
                                               const std::string& key) {
  if (send_cmd_no_reply<string>(
          redis, {"SELECT", std::to_string(REDIS_MODEL_DB_NUM)})) {
    auto model_data = send_cmd_vec_reply(redis, {"HGETALL", key});
    return parse_redis_map(model_data);
  } else {
    return unordered_map<string, string>{};
  }
}

bool insert_container(Redox& redis, const VersionedModelId& model_id,
                      int model_replica_id, int zmq_connection_id,
                      InputType input_type) {
  if (send_cmd_no_reply<string>(
          redis, {"SELECT", std::to_string(REDIS_CONTAINER_DB_NUM)})) {
    std::string replica_key = gen_model_replica_key(model_id, model_replica_id);
    std::string model_id_key = gen_versioned_model_key(model_id);
    vector<string> cmd_vec{
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
                      int model_replica_id) {
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
                                            int model_replica_id) {
  if (send_cmd_no_reply<string>(
          redis, {"SELECT", std::to_string(REDIS_CONTAINER_DB_NUM)})) {
    std::string replica_key = gen_model_replica_key(model_id, model_replica_id);
    auto container_data = send_cmd_vec_reply(redis, {"HGETALL", replica_key});
    return parse_redis_map(container_data);
  } else {
    return unordered_map<string, string>{};
  }
}

unordered_map<string, string> get_container_by_key(Redox& redis,
                                                   const std::string& key) {
  if (send_cmd_no_reply<string>(
          redis, {"SELECT", std::to_string(REDIS_CONTAINER_DB_NUM)})) {
    auto container_data = send_cmd_vec_reply(redis, {"HGETALL", key});
    return parse_redis_map(container_data);
  } else {
    return unordered_map<string, string>{};
  }
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

}  // namespace redis
}  // namespace clipper
