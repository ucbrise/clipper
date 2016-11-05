
// #include <clipper/datatypes.hpp>
#include <clipper/persistent_state.hpp>

namespace clipper {

size_t state_key_hash(const StateKey& key) {
  return std::hash<std::string>()(std::get<0>(key)) ^
         std::hash<long>()(std::get<1>(key)) ^
         std::hash<long>()(std::get<2>(key));
}

boost::optional<ByteBuffer> StateDB::get(const StateKey& key) const {
  auto loc = state_table_.find(key);
  if (loc == state_table_.end()) {
    return boost::none;
  } else {
    return loc->second;
  }
}

void StateDB::put(StateKey key, ByteBuffer value) { state_table_[key] = value; }

}  // namespace clipper
