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
  // std::cout << "FOUND MODEL: {" << std::endl;
  for (auto m = redis_data.begin(); m != redis_data.end(); ++m) {
    auto key = *m;
    m += 1;
    auto value = *m;
    std::cout << "\t" << key << ": " << value << std::endl;
    parsed_map[key] = value;
  }
  // std::cout << "}" << std::endl;
  return parsed_map;
}

size_t model_replica_hash(const VersionedModelId& key, int model_replica_id) {
  return std::hash<std::string>()(key.first) ^ std::hash<int>()(key.second) ^
         std::hash<int>()(model_replica_id);
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

/// Gets a model from the Redis model table
bool insert_model(Redox& redis, const VersionedModelId& model_id,
                  const vector<string>& labels) {
  if (send_cmd_no_reply<string>(
          redis, {"SELECT", std::to_string(REDIS_MODEL_DB_NUM)})) {
    size_t model_id_key = versioned_model_hash(model_id);
    vector<string> cmd_vec{"HMSET",         std::to_string(model_id_key),
                           "model_name",    model_id.first,
                           "model_version", std::to_string(model_id.second),
                           "load",          std::to_string(0.0),
                           "labels",        labels_to_str(labels)};
    return send_cmd_no_reply<string>(redis, cmd_vec);
  } else {
    return false;
  }
}

/// Gets a model from the Redis model table
bool delete_model(Redox& redis, const VersionedModelId& model_id) {
  if (send_cmd_no_reply<string>(
          redis, {"SELECT", std::to_string(REDIS_MODEL_DB_NUM)})) {
    size_t model_id_key = versioned_model_hash(model_id);
    return send_cmd_no_reply<int>(redis, {"DEL", std::to_string(model_id_key)});
  } else {
    return false;
  }
}

/// Gets a model from the Redis model table. Map corresponds to
/// model schema.
unordered_map<string, string> get_model(Redox& redis,
                                        const VersionedModelId& model_id) {
  if (send_cmd_no_reply<string>(
          redis, {"SELECT", std::to_string(REDIS_MODEL_DB_NUM)})) {
    size_t model_id_key = versioned_model_hash(model_id);
    auto model_data =
        send_cmd_vec_reply(redis, {"HGETALL", std::to_string(model_id_key)});

    // std::unordered_map<string, string> model_map;
    // std::cout << "FOUND MODEL: {" << std::endl;
    // for (auto m = model_data.begin(); m != model_data.end(); ++m) {
    //   auto key = *m;
    //   m += 1;
    //   auto value = *m;
    //   std::cout << "\t" << key << ": " << value << std::endl;
    //   model_map[key] = value;
    // }
    // std::cout << "}" << std::endl;
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
    // std::unordered_map<string, string> model_map;
    // std::cout << "FOUND MODEL: {" << std::endl;
    // for (auto m = model_data.begin(); m != model_data.end(); ++m) {
    //   auto key = *m;
    //   m += 1;
    //   auto value = *m;
    //   std::cout << "\t" << key << ": " << value << std::endl;
    //   model_map[key] = value;
    // }
    // std::cout << "}" << std::endl;
    // return model_map;
    return parse_redis_map(model_data);
  } else {
    return unordered_map<string, string>{};
  }
}

bool insert_container(Redox& redis, const VersionedModelId& model_id,
                      int model_replica_id, int zmq_connection_id) {
  if (send_cmd_no_reply<string>(
          redis, {"SELECT", std::to_string(REDIS_CONTAINER_DB_NUM)})) {
    size_t replica_key = model_replica_hash(model_id, model_replica_id);
    size_t model_id_key = versioned_model_hash(model_id);
    vector<string> cmd_vec{"HMSET",
                           std::to_string(replica_key),
                           "model_id",
                           std::to_string(model_id_key),
                           "model_name",
                           model_id.first,
                           "model_version",
                           std::to_string(model_id.second),
                           "model_replica_id",
                           std::to_string(model_replica_id),
                           "zmq_connection_id",
                           std::to_string(zmq_connection_id),
                           "batch_size",
                           std::to_string(1)};
    return send_cmd_no_reply<string>(redis, cmd_vec);
  } else {
    return false;
  }
}

bool delete_container(Redox& redis, const VersionedModelId& model_id,
                      int model_replica_id) {
  if (send_cmd_no_reply<string>(
          redis, {"SELECT", std::to_string(REDIS_CONTAINER_DB_NUM)})) {
    size_t replica_key = model_replica_hash(model_id, model_replica_id);
    return send_cmd_no_reply<int>(redis, {"DEL", std::to_string(replica_key)});
  } else {
    return false;
  }
}

unordered_map<string, string> get_container(Redox& redis,
                                            const VersionedModelId& model_id,
                                            int model_replica_id) {
  if (send_cmd_no_reply<string>(
          redis, {"SELECT", std::to_string(REDIS_CONTAINER_DB_NUM)})) {
    size_t replica_key = model_replica_hash(model_id, model_replica_id);
    auto container_data =
        send_cmd_vec_reply(redis, {"HGETALL", std::to_string(replica_key)});
    // std::unordered_map<string, string> container_map;
    // std::cout << "FOUND CONTAINER: {" << std::endl;
    // for (auto c = container_data.begin(); c != container_data.end(); ++c) {
    //   auto key = *c;
    //   c += 1;
    //   auto value = *m;
    //   std::cout << "\t" << key << ": " << value << std::endl;
    //   container_map[key] = value;
    // }
    // std::cout << "}" << std::endl;
    // return container_map;
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
    // std::unordered_map<string, string> container_map;
    // std::cout << "FOUND CONTAINER: {" << std::endl;
    // for (auto c = container_data.begin(); c != container_data.end(); ++c) {
    //   auto key = *c;
    //   c += 1;
    //   auto value = *m;
    //   std::cout << "\t" << key << ": " << value << std::endl;
    //   container_map[key] = value;
    // }
    // std::cout << "}" << std::endl;
    // return container_map;
    return parse_redis_map(container_data);
  } else {
    return unordered_map<string, string>{};
  }
}

void subscribe_to_keyspace_changes(
    int db, Subscriber& subscriber,
    std::function<void(const std::string&)> callback) {
  std::ostringstream subscription;
  subscription << "__keyspace@" << std::to_string(db) << "__:*";
  // subscription << "__key*__:*";
  std::string sub_str = subscription.str();
  std::cout << "SUBSCRIPTION STRING: " << sub_str << std::endl;
  subscriber.psubscribe(
      sub_str, [callback](const std::string& topic, const std::string& msg) {
        size_t split_idx = topic.find_first_of(":");
        std::string key = topic.substr(split_idx + 1);
        std::cout << "MESSAGE: " << msg << std::endl;
        callback(key);
      });
}

void subscribe_to_model_changes(
    Subscriber& subscriber, std::function<void(const std::string&)> callback) {
  subscribe_to_keyspace_changes(REDIS_MODEL_DB_NUM, subscriber,
                                std::move(callback));
}

void subscribe_to_container_changes(
    Subscriber& subscriber, std::function<void(const std::string&)> callback) {
  subscribe_to_keyspace_changes(REDIS_CONTAINER_DB_NUM, subscriber,
                                std::move(callback));
}

}  // namespace redis
}  // namespace clipper
