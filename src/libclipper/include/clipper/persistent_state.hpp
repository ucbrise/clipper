#ifndef CLIPPER_LIB_PERSISTENT_STATE_H
#define CLIPPER_LIB_PERSISTENT_STATE_H

#include <atomic>
#include <functional>
#include <shared_mutex>
#include <tuple>
#include <unordered_map>

#include <boost/optional.hpp>
#include <redox.hpp>

#include "datatypes.hpp"

namespace clipper {

// The entries in the key are query_label, user_id, model_hash
using StateKey = std::tuple<std::string, long, long>;

size_t state_key_hash(const StateKey& key);

// using StateMap =
//     std::unordered_map<StateKey, ByteBuffer, decltype(&state_key_hash)>;

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

  bool init();

  // Non-const because we need to lock a mutex
  boost::optional<std::string> get(const StateKey& key);

  bool put(StateKey key, std::string value);

  bool delete_key(StateKey key);

 private:
  std::atomic<bool> initialized_;
  // StateMap state_table_{5, state_key_hash};
  redox::Redox redis_connection_;
};

}  // namespace clipper

#endif  // CLIPPER_LIB_PERSISTENT_STATE_H
