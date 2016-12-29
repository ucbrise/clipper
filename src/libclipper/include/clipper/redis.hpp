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

/**
 * Issues a command to Redis and checks return code.
 * Returns true if the command was successful.
*/
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

std::string gen_model_replica_key(const VersionedModelId& key,
                                  int model_replica_id);

std::string gen_versioned_model_key(const VersionedModelId& key);

std::string labels_to_str(const std::vector<std::string>& labels);

std::vector<std::string> str_to_labels(const std::string& label_str);

/**
 * Inserts a model into the model table. This will
 * overwrite any existing entry with the same key.
 */
bool insert_model(redox::Redox& redis, const VersionedModelId& model_id,
                  const std::vector<std::string>& labels);

/**
 * Deletes a model from the model table if it exists.
 * @return Returns true if the model was present in the table
 * and was successfully deleted. Returns false if there was a problem
 * or if the model was not in the table.
 */
bool delete_model(redox::Redox& redis, const VersionedModelId& model_id);

/**
 * Looks up a model based on its model ID. This
 * is the canonical way of uniquely identifying a model in
 * the rest of the Clipper codebase.
 */
std::unordered_map<std::string, std::string> get_model(
    redox::Redox& redis, const VersionedModelId& model_id);

/**
 * Looks up an entry in the model table by the fully
 * specified Redis key.
 *
 * This function is primarily used for looking up a model
 * entry after a subscriber detects a change to the model
 * with the given key.
 */
std::unordered_map<std::string, std::string> get_model_by_key(
    redox::Redox& redis, const std::string& key);

/**
 * Inserts a container into the container table. This will
 * overwrite any existing entry with the same key.
 */
bool insert_container(redox::Redox& redis, const VersionedModelId& model_id,
                      int model_replica_id, int zmq_connection_id,
                      InputType input_type);

/**
 * Deletes a container from the container table if it exists.
 * @return Returns true if the container was present in the table
 * and was successfully deleted. Returns false if there was a problem
 * or if the container was not in the table.
 */
bool delete_container(redox::Redox& redis, const VersionedModelId& model_id,
                      int model_replica_id);

/**
 * Looks up a container based on its model and replica IDs. This
 * is the canonical way of uniquely identifying a container in
 * the rest of the Clipper codebase.
 */
std::unordered_map<std::string, std::string> get_container(
    redox::Redox& redis, const VersionedModelId& model_id,
    int model_replica_id);

/**
 * Looks up an entry in the container table by the fully
 * specified Redis key.
 *
 * This function is primarily used for looking up a container
 * entry after a subscriber detects a change to the container
 * with the given key.
 */
std::unordered_map<std::string, std::string> get_container_by_key(
    redox::Redox& redis, const std::string& key);

/**
* Subscribes to changes in the model table. The
* callback is called with the string key of the model
* that changed and the Redis event type. The key can
* be used to look up the new value. The message type identifies
* what type of change was detected. This allows subscribers
* to differentiate between inserts, updates, and deletes if necessary.
*/
void subscribe_to_model_changes(
    redox::Subscriber& subscriber,
    std::function<void(const std::string&, const std::string&)> callback);
/**
 * Subscribes to changes in the container table. The
 * callback is called with the string key of the container
 * that changed and the Redis event type. The key can
 * be used to look up the new value. The message type identifies
 * what type of change was detected. This allows subscribers
 * to differentiate between inserts, updates, and deletes if necessary.
*/
void subscribe_to_container_changes(
    redox::Subscriber& subscriber,
    std::function<void(const std::string&, const std::string&)> callback);

}  // namespace redis
}  // namespace clipper

#endif  // CLIPPER_LIB_REDIS_HPP
