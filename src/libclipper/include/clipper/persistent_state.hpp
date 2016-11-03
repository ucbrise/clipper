#ifndef CLIPPER_LIB_PERSISTENT_STATE_H
#define CLIPPER_LIB_PERSISTENT_STATE_H

#include <boost/optional.hpp>
#include <tuple>

#include <clipper/datatypes.hpp>

namespace clipper {

// The entries in the key are query_label, user_id, model_hash
using StateKey = std::tuple<std::string, long, long>;

class StateDB {
 public:

  boost::optional<ByteBuffer> get(Statekey key) const;

  void put(Statekey key, ByteBuffer value);

 private:
  std::unordered_map<StateKey, ByteBuffer> state_table_;
};

}  // namespace clipper

#endif  // CLIPPER_LIB_PERSISTENT_STATE_H
