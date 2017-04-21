#ifndef CLIPPER_LIB_REDIS_HPP
#define CLIPPER_LIB_REDIS_HPP

#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <boost/optional.hpp>
#include <clipper/logging.hpp>
#include <redox.hpp>

#include "constants.hpp"
#include "datatypes.hpp"

// a collection of redis utilities for common operations

namespace clipper {
namespace redis {

const std::string LOGGING_TAG_REDIS = "REDIS";

/**
 * Issues a command to Redis and checks return code.
 * \return Returns true if the command was successful.
*/
template <class ReplyT>
bool send_cmd_no_reply(redox::Redox& redis,
                       const std::vector<std::string>& cmd_vec) {
  bool ok = true;
  redox::Command<ReplyT>& cmd = redis.commandSync<ReplyT>(cmd_vec);
  if (!cmd.ok()) {
    log_error_formatted(LOGGING_TAG_REDIS, "Error with command \"{}\": {}",
                        redis.vecToStr(cmd_vec), cmd.lastError());
    ok = false;
  } else {
    log_info_formatted(LOGGING_TAG_REDIS, "Successfully issued command \"{}\"",
                       redis.vecToStr(cmd_vec));
  }
  cmd.free();
  return ok;
}

template <class ReplyT>
boost::optional<ReplyT> send_cmd_with_reply(
    redox::Redox& redis, const std::vector<std::string>& cmd_vec) {
  redox::Command<ReplyT>& cmd = redis.commandSync<ReplyT>(cmd_vec);
  if (!cmd.ok()) {
    log_error_formatted(LOGGING_TAG_REDIS, "Error with command \"{}\": {}",
                        redis.vecToStr(cmd_vec), cmd.lastError());
  } else {
    log_info_formatted(LOGGING_TAG_REDIS, "Successfully issued command \"{}\"",
                       redis.vecToStr(cmd_vec));
    return cmd.reply();
  }
  cmd.free();
  return boost::none;
}

/**
 * Generates a unique, human-interpretable key for a model replica.
 * Intended for use as a primary key in a database table.
 */
std::string gen_model_replica_key(const VersionedModelId& key,
                                  const int model_replica_id);

/**
 * Generates a unique, human-interpretable key for a versioned model.
 * Intended for use as a primary key in a database table.
 */
std::string gen_versioned_model_key(const VersionedModelId& key);

std::string gen_model_current_version_key(const std::string& model_name);

std::string labels_to_str(const std::vector<std::string>& labels);

std::vector<std::string> str_to_labels(const std::string& label_str);

std::string model_names_to_str(const std::vector<std::string>& names);

std::vector<std::string> str_to_model_names(const std::string& names_str);

std::string models_to_str(const std::vector<VersionedModelId>& models);

std::vector<VersionedModelId> str_to_models(const std::string& model_str);

bool set_current_model_version(redox::Redox& redis,
                               const std::string& model_name, int version);

int get_current_model_version(redox::Redox& redis,
                              const std::string& model_name);

/**
 * Adds a model into the model table. This will
 * overwrite any existing entry with the same key.
 *
 * \param container_name should be the name of a Docker container
 * \param model_data_path should be the path on the Clipper host
 * to the serialized model data needed for the container
 *
 * \return Returns true if the add was successful.
 */
bool add_model(redox::Redox& redis, const VersionedModelId& model_id,
               const InputType& input_type,
               const std::vector<std::string>& labels,
               const std::string& container_name,
               const std::string& model_data_path);

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
 * model can be found in the source for `add_model()`. If the
 * model was not found, an empty map will be returned.
 */
std::unordered_map<std::string, std::string> get_model(
    redox::Redox& redis, const VersionedModelId& model_id);

/**
 * Looks up all available versions for a model.
 *
 * \return Returns a list of model versions. If the
 * model was not found, an empty list will be returned.
 */
std::vector<int> get_model_versions(redox::Redox& redis,
                                    const std::string& model_name);

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
 * model can be found in the source for `add_model()`. If the
 * model was not found, an empty map will be returned.
 */
std::unordered_map<std::string, std::string> get_model_by_key(
    redox::Redox& redis, const std::string& key);

/**
 * Adds a container into the container table. This will
 * overwrite any existing entry with the same key.
 *
 * \return Returns true of the add was successful.
 */
bool add_container(redox::Redox& redis, const VersionedModelId& model_id,
                   const int model_replica_id, const int zmq_connection_id,
                   const InputType& input_type);

/**
 * Deletes a container from the container table if it exists.
 *
 * \return Returns true if the container was present in the table
 * and was successfully deleted. Returns false if there was a problem
 * or if the container was not in the table.
 */
bool delete_container(redox::Redox& redis, const VersionedModelId& model_id,
                      const int model_replica_id);

/**
 * Looks up a container based on its model and replica IDs. This
 * is the canonical way of uniquely identifying a container in
 * the rest of the Clipper codebase.
 *
 * \return Returns a map of container attribute name-value pairs as
 * strings. Any parsing of the attribute values from their string
 * format (e.g. to a numerical representation) must be done by the
 * caller of this function. The set of attributes stored for a
 * container can be found in the source for `add_container()`. If the
 * container was not found, an empty map will be returned.
 */
std::unordered_map<std::string, std::string> get_container(
    redox::Redox& redis, const VersionedModelId& model_id,
    const int model_replica_id);

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
 * container can be found in the source for `add_container()`. If the
 * container was not found, an empty map will be returned.
 */
std::unordered_map<std::string, std::string> get_container_by_key(
    redox::Redox& redis, const std::string& key);

/**
 * Adds an application into the application table. This will
 * overwrite any existing entry with the same key.
 *
 * \return Returns true of the add was successful.
 */
bool add_application(redox::Redox& redis, const std::string& appname,
                     const std::vector<std::string>& models,
                     const InputType& input_type, const std::string& policy,
                     const std::string& default_output,
                     const long latency_slo_micros);

/**
 * Deletes a container from the container table if it exists.
 *
 * \return Returns true if the container was present in the table
 * and was successfully deleted. Returns false if there was a problem
 * or if the application was not in the table.
 */
bool delete_application(redox::Redox& redis, const std::string& appname);

/**
 * Lists the names of all applications registered with Clipper.
 *
 * \return Returns a vector of application names as strings. If no
 * applications were found, an empty vector will be returned.
 */
std::vector<std::string> list_application_names(redox::Redox& redis);

/**
 * Looks up an application based on its name.
 *
 * \return Returns a map of application attribute name-value pairs as
 * strings. Any parsing of the attribute values from their string
 * format (e.g. to a numerical representation) must be done by the
 * caller of this function. The set of attributes stored for a
 * application can be found in the source for `add_application()`. If the
 * application was not found, an empty map will be returned.
 */
std::unordered_map<std::string, std::string> get_application(
    redox::Redox& redis, const std::string& appname);

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
 * application can be found in the source for `add_application()`. If the
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
* to differentiate between adds, updates, and deletes if necessary.
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
 * to differentiate between adds, updates, and deletes if necessary.
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
 * to differentiate between adds, updates, and deletes if necessary.
*/
void subscribe_to_application_changes(
    redox::Subscriber& subscriber,
    std::function<void(const std::string&, const std::string&)> callback);

/**
* Subscribes to changes in model versions.
*
* The callback is called with the string key of the model
* that changed and the Redis event type. The key can
* be used to look up the new value. The message type identifies
* what type of change was detected. This allows subscribers
* to differentiate between adds, updates, and deletes if necessary.
*/
void subscribe_to_model_version_changes(
    redox::Subscriber& subscriber,
    std::function<void(const std::string&, const std::string&)> callback);

}  // namespace redis
}  // namespace clipper

#endif  // CLIPPER_LIB_REDIS_HPP
