#ifndef CLIPPER_LIB_PERSISTENT_STATE_HPP
#define CLIPPER_LIB_PERSISTENT_STATE_HPP

#include <atomic>
#include <functional>
#include <shared_mutex>
#include <tuple>
#include <unordered_map>

#include <boost/optional.hpp>
#include <redox.hpp>

#include "constants.hpp"
#include "datatypes.hpp"

namespace clipper {

const std::string LOGGING_TAG_STATE_DB = "STATEDB";

// The entries in the key are query_label, user_id, model_hash
using StateKey = std::tuple<std::string, long, long>;

size_t state_key_hash(const StateKey& key);

// Threadsafe, non-copyable state storage
class StateDB {
 public:
  StateDB();
  ~StateDB();

  // Disallow copies because of the mutex
  StateDB(const StateDB&) = delete;
  StateDB& operator=(const StateDB&) = delete;

  StateDB(StateDB&&) = default;

  StateDB& operator=(StateDB&&) = default;

  /**
   * Get the value associated with the key if present
   * in the DB.
   *
   * @return Returns boost::none if the key is not found.
   */
  boost::optional<std::string> get(const StateKey& key);

  /**
   * Puts the key-value pair into the DB. If the key already
   * exists in the DB, the new value will blindly overwrite the old
   * value.
   *
   * @return Logs an error and returns false if there was an unexpected
   * error with the put.
   */
  bool put(StateKey key, std::string value);

  /**
   * Removes the entry associated with the key from the DB if present.
   * If the key is not present in the DB, this method has no effect.
   *
   * @return Logs an error and returns false if there was an unexpected
   * error with the removal.
   */
  bool remove(StateKey key);

  /**
   * Returns the total number of keys in the DB.
   */
  int num_entries();

 private:
  redox::Redox redis_connection_;
};

}  // namespace clipper

#endif  // CLIPPER_LIB_PERSISTENT_STATE_HPP
