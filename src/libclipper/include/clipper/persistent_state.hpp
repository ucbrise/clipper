#ifndef CLIPPER_LIB_PERSISTENT_STATE_H
#define CLIPPER_LIB_PERSISTENT_STATE_H

#include <boost/optional.hpp>
#include <functional>
#include <tuple>
#include <unordered_map>

#include "datatypes.hpp"

namespace clipper {

// The entries in the key are query_label, user_id, model_hash
using StateKey = std::tuple<std::string, long, long>;

size_t state_key_hash(const StateKey& key);

using StateMap =
    std::unordered_map<StateKey, ByteBuffer, decltype(&state_key_hash)>;

class StateDB {
 public:
  boost::optional<ByteBuffer> get(const StateKey& key) const;

  StateDB() = default;

  void put(StateKey key, ByteBuffer value);

 private:
  StateMap state_table_{StateMap(5, state_key_hash)};
};

}  // namespace clipper

#endif  // CLIPPER_LIB_PERSISTENT_STATE_H
