
#include <iostream>

// #include <clipper/datatypes.hpp>
#include <boost/thread.hpp>
#include <clipper/persistent_state.hpp>

namespace clipper {

size_t state_key_hash(const StateKey& key) {
  return std::hash<std::string>()(std::get<0>(key)) ^
         std::hash<long>()(std::get<1>(key)) ^
         std::hash<long>()(std::get<2>(key));
}

StateDB::StateDB() { std::cout << "Persistent state DB created" << std::endl; }

boost::optional<ByteBuffer> StateDB::get(const StateKey& key) {
  boost::shared_lock<boost::shared_mutex> l{m_};
  auto loc = state_table_.find(key);
  if (loc == state_table_.end()) {
    return boost::none;
  } else {
    return loc->second;
  }
}

void StateDB::put(StateKey key, ByteBuffer value) {
  boost::unique_lock<boost::shared_mutex> l{m_};
  state_table_[key] = value;
}

}  // namespace clipper
