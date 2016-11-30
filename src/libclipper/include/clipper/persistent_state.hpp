#ifndef CLIPPER_LIB_PERSISTENT_STATE_H
#define CLIPPER_LIB_PERSISTENT_STATE_H

#include <boost/optional.hpp>
#include <functional>
#include <tuple>
#include <unordered_map>
// #include <shared_mutex>
#include <mutex>

#include "datatypes.hpp"

namespace clipper {

// The entries in the key are query_label, user_id, model_hash
using StateKey = std::tuple<std::string, long, long>;

size_t state_key_hash(const StateKey& key);

using StateMap =
    std::unordered_map<StateKey, ByteBuffer, decltype(&state_key_hash)>;

// Threadsafe, non-copyable state storage
class StateDB {
 public:
  StateDB();
  ~StateDB() = default;

  // Disallow copies because of the mutex
  StateDB(const StateDB&) = delete;
  StateDB& operator=(const StateDB&) = delete;

  StateDB(StateDB&&) = default;

  StateDB& operator=(StateDB&&) = default;

  // Non-const because we need to lock a mutex
  boost::optional<ByteBuffer> get(const StateKey& key);

  void put(StateKey key, ByteBuffer value);

 private:
  // TODO(shared_mutex): replace m_ with a shared_mutex
  // std::shared_timed_mutex m_;
  std::mutex m_;
  StateMap state_table_{5, state_key_hash};
};

}  // namespace clipper

#endif  // CLIPPER_LIB_PERSISTENT_STATE_H
