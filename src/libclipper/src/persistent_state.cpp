
#include <iostream>

// #include <clipper/datatypes.hpp>
#include <clipper/persistent_state.hpp>

namespace clipper {

size_t state_key_hash(const StateKey& key) {
  return std::hash<std::string>()(std::get<0>(key)) ^
         std::hash<long>()(std::get<1>(key)) ^
         std::hash<long>()(std::get<2>(key));
}

StateDB::StateDB() { std::cout << "Persistent state DB created" << std::endl; }

boost::optional<ByteBuffer> StateDB::get(const StateKey& key) {
  // std::shared_lock<std::shared_timed_mutex> m_;
  std::unique_lock<std::mutex> m_;
  auto loc = state_table_.find(key);
  if (loc == state_table_.end()) {
    return boost::none;
  } else {
    return loc->second;
  }
}

void StateDB::put(StateKey key, ByteBuffer value) {
  // std::unique_lock<std::shared_timed_mutex> m_;
  std::unique_lock<std::mutex> m_;
  state_table_[key] = value;
}

}  // namespace clipper
