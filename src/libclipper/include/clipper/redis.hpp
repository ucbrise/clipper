#ifndef CLIPPER_LIB_REDIS_HPP
#define CLIPPER_LIB_REDIS_HPP

#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <redox.hpp>

#include "constants.hpp"
#include "datatypes.hpp"

// a collection of redis utilities for common operations

namespace clipper {

namespace redis {

// /// Issues a command to Redis and checks return code.
// /// Returns true if the command was successful.
// bool send_cmd_no_reply(std::shared_ptr<redox::Redis> redis,
//                        std::vector<std::string> cmd_vec);
//
// std::vector<std::string> send_cmd_vec_reply(std::shared_ptr<redox::Redis>
// redis,
//                                             std::vector<std::string>
//                                             cmd_vec);

/// Issues a command to Redis and checks return code.
/// Returns true if the command was successful.
template <class ReplyT>
bool send_cmd_no_reply(redox::Redox& redis, std::vector<std::string> cmd_vec) {
  bool ok = true;
  redox::Command<ReplyT>& cmd = redis.commandSync<ReplyT>(cmd_vec);
  if (!cmd.ok()) {
    std::cerr << "Error with command \"" << redis.vecToStr(cmd_vec)
              << "\": " << cmd.lastError() << std::endl;
    ok = false;
  } else {
    std::cout << "Successfully issued command \"" << redis.vecToStr(cmd_vec)
              << "\": " << std::endl;
  }
  cmd.free();
  return ok;
}

size_t model_replica_hash(const VersionedModelId& key, int model_replica_id);

std::string labels_to_str(const std::vector<std::string>& labels);

std::vector<std::string> str_to_labels(const std::string& label_str);

/// Gets a model from the Redis model table
bool insert_model(redox::Redox& redis, const VersionedModelId& model_id,
                  const std::vector<std::string>& labels);

/// Gets a model from the Redis model table
bool delete_model(redox::Redox& redis, const VersionedModelId& model_id);

/// Gets a model from the Redis model table. Map corresponds to
/// model schema.
std::unordered_map<std::string, std::string> get_model(
    redox::Redox& redis, const VersionedModelId& model_id);

std::unordered_map<std::string, std::string> get_model_by_key(
    redox::Redox& redis, const std::string& key);

bool insert_container(redox::Redox& redis, const VersionedModelId& model_id,
                      int model_replica_id, int zmq_connection_id,
                      InputType input_type);

bool delete_container(redox::Redox& redis, const VersionedModelId& model_id,
                      int model_replica_id);

std::unordered_map<std::string, std::string> get_container(
    redox::Redox& redis, const VersionedModelId& model_id,
    int model_replica_id);

std::unordered_map<std::string, std::string> get_container_by_key(
    redox::Redox& redis, const std::string& key);

// Subscribes to changes in the container table. The
// callback is called with the string key of the container
// that changed, which can then be used to look up the
// affected row.
void subscribe_to_model_changes(
    redox::Subscriber& subscriber,
    std::function<void(const std::string&)> callback);

// Subscribes to changes in the container table. The
// callback is called with the string key of the container
// that changed, which can then be used to look up the
// affected row.
void subscribe_to_container_changes(
    redox::Subscriber& subscriber,
    std::function<void(const std::string&)> callback);

}  // namespace redis

}  // namespace clipper

#endif  // CLIPPER_LIB_REDIS_HPP
