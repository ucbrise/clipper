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
 * \return Returns true if the command was successful.
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

/**
 * Generates a unique, human-interpretable key for a model replica.
 * Intended for use as a primary key in a database table.
 */
std::string gen_model_replica_key(const VersionedModelId& key,
                                  int model_replica_id);

/**
 * Generates a unique, human-interpretable key for a versioned model.
 * Intended for use as a primary key in a database table.
 */
std::string gen_versioned_model_key(const VersionedModelId& key);

std::string labels_to_str(const std::vector<std::string>& labels);

std::vector<std::string> str_to_labels(const std::string& label_str);

std::string models_to_str(const std::vector<VersionedModelId>& models);

std::vector<VersionedModelId> str_to_models(const std::string& model_str);

/**
 * Inserts a model into the model table. This will
 * overwrite any existing entry with the same key.
 *
 * \return Returns true if the insert was successful.
 */
bool insert_model(redox::Redox& redis, const VersionedModelId& model_id,
                  InputType input_type, std::string output_type,
                  const std::vector<std::string>& labels);

/**
 * Deletes a model from the model table if it exists.
 *
 * \return Returns true if the model was present in the table
 * and was successfully deleted. Returns false if there was a problem
 * or if the model was not in the table.
 */
bool delete_model(redox::Redox& redis, const VersionedModelId& model_id);

/**
 * Looks up a model based on its model ID. This
 * is the canonical way of uniquely identifying a model in
 * the rest of the Clipper codebase.
 *
 * \return Returns a map of model attribute name-value pairs as
 * strings. Any parsing of the attribute values from their string
 * format (e.g. to a numerical representation) must be done by the
 * caller of this function. The set of attributes stored for a
 * model can be found in the source for `insert_model()`. If the
 * model was not found, an empty map will be returned.
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
 *
 * \return Returns a map of model attribute name-value pairs as
 * strings. Any parsing of the attribute values from their string
 * format (e.g. to a numerical representation) must be done by the
 * caller of this function. The set of attributes stored for a
 * model can be found in the source for `insert_model()`. If the
 * model was not found, an empty map will be returned.
 */
std::unordered_map<std::string, std::string> get_model_by_key(
    redox::Redox& redis, const std::string& key);

/**
 * Inserts a container into the container table. This will
 * overwrite any existing entry with the same key.
 *
 * \return Returns true of the insert was successful.
 */
bool insert_container(redox::Redox& redis, const VersionedModelId& model_id,
                      int model_replica_id, int zmq_connection_id,
                      InputType input_type);

/**
 * Deletes a container from the container table if it exists.
 *
 * \return Returns true if the container was present in the table
 * and was successfully deleted. Returns false if there was a problem
 * or if the container was not in the table.
 */
bool delete_container(redox::Redox& redis, const VersionedModelId& model_id,
                      int model_replica_id);

/**
 * Looks up a container based on its model and replica IDs. This
 * is the canonical way of uniquely identifying a container in
 * the rest of the Clipper codebase.
 *
 * \return Returns a map of container attribute name-value pairs as
 * strings. Any parsing of the attribute values from their string
 * format (e.g. to a numerical representation) must be done by the
 * caller of this function. The set of attributes stored for a
 * container can be found in the source for `insert_container()`. If the
 * container was not found, an empty map will be returned.
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
 *
 * \return Returns a map of container attribute name-value pairs as
 * strings. Any parsing of the attribute values from their string
 * format (e.g. to a numerical representation) must be done by the
 * caller of this function. The set of attributes stored for a
 * container can be found in the source for `insert_container()`. If the
 * container was not found, an empty map will be returned.
 */
std::unordered_map<std::string, std::string> get_container_by_key(
    redox::Redox& redis, const std::string& key);

/**
 * Inserts an application into the application table. This will
 * overwrite any existing entry with the same key.
 *
 * \return Returns true of the insert was successful.
 */
bool insert_application(redox::Redox& redis, std::string name,
                        std::vector<VersionedModelId> models,
                        InputType input_type, std::string output_type,
                        std::string policy, long latency_slo_micros);

/**
 * Deletes a container from the container table if it exists.
 *
 * \return Returns true if the container was present in the table
 * and was successfully deleted. Returns false if there was a problem
 * or if the application was not in the table.
 */
bool delete_application(redox::Redox& redis, std::string name);

/**
 * Looks up an application based on its name.
 *
 * \return Returns a map of application attribute name-value pairs as
 * strings. Any parsing of the attribute values from their string
 * format (e.g. to a numerical representation) must be done by the
 * caller of this function. The set of attributes stored for a
 * application can be found in the source for `insert_application()`. If the
 * application was not found, an empty map will be returned.
 */
std::unordered_map<std::string, std::string> get_application(
    redox::Redox& redis, std::string name);

/**
 * Looks up an entry in the application table by the fully
 * specified Redis key.
 *
 * This function is primarily used for looking up a application
 * entry after a subscriber detects a change to the application
 * with the given key.
 *
 * \return Returns a map of application attribute name-value pairs as
 * strings. Any parsing of the attribute values from their string
 * format (e.g. to a numerical representation) must be done by the
 * caller of this function. The set of attributes stored for a
 * application can be found in the source for `insert_application()`. If the
 * application was not found, an empty map will be returned.
 */
std::unordered_map<std::string, std::string> get_application_by_key(
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

/**
 * Subscribes to changes in the application table. The
 * callback is called with the string key of the application
 * that changed and the Redis event type. The key can
 * be used to look up the new value. The message type identifies
 * what type of change was detected. This allows subscribers
 * to differentiate between inserts, updates, and deletes if necessary.
*/
void subscribe_to_application_changes(
    redox::Subscriber& subscriber,
    std::function<void(const std::string&, const std::string&)> callback);

}  // namespace redis
}  // namespace clipper

#endif  // CLIPPER_LIB_REDIS_HPP
