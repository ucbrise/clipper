
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

// #include <clipper/datatypes.hpp>
#include <boost/thread.hpp>
#include <clipper/persistent_state.hpp>

namespace clipper {

constexpr int REDIS_STATE_TABLE = 3;

size_t state_key_hash(const StateKey& key) {
  return std::hash<std::string>()(std::get<0>(key)) ^
         std::hash<long>()(std::get<1>(key)) ^
         std::hash<long>()(std::get<2>(key));
}

StateDB::StateDB() : initialized_(false) {
  std::cout << "Persistent state DB created" << std::endl;
}

bool StateDB::init() {
  while (!redis_connection_.connect("localhost", 6379)) {
    std::cout << "ERROR connecting to Redis" << std::endl;
    std::cout << "Sleeping 1 second..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  redox::Command<std::string>& set_table_cmd =
      redis_connection_.commandSync<std::string>(
          {"SELECT", std::to_string(REDIS_STATE_TABLE)});
  if (!set_table_cmd.ok()) {
    std::cerr << "Error selecting state table DB" << std::endl;
  } else {
    std::cout << "Success connecting to Redis" << std::endl;
    initialized_ = true;
  }
  set_table_cmd.free();
  return initialized_;
}

StateDB::~StateDB() {
  if (initialized_) {
    redis_connection_.disconnect();
  }
  initialized_ = false;
}

boost::optional<std::string> StateDB::get(const StateKey& key) {
  if (!initialized_) {
    std::cout << "Cannot get state from uninitialized StateDB" << std::endl;
    return boost::none;
  }
  size_t hash = state_key_hash(key);
  redox::Command<std::string>& cmd =
      redis_connection_.commandSync<std::string>({"GET", std::to_string(hash)});
  if (cmd.ok()) {
    std::cout << "Found " << cmd.reply() << " in Redis StateDB" << std::endl;
    return cmd.reply();
  } else {
    if (cmd.status() != redox::Command<std::string>::NIL_REPLY) {
      std::cerr << "Error looking up state: " << cmd.lastError() << std::endl;
    }
    return boost::none;
  }
}

bool StateDB::put(StateKey key, std::string value) {
  if (!initialized_) {
    std::cout << "Cannot put state into uninitialized StateDB" << std::endl;
    return false;
  } else {
    size_t hash = state_key_hash(key);
    redox::Command<std::string>& cmd =
        redis_connection_.commandSync<std::string>(
            {"SET", std::to_string(hash), value});
    if (!cmd.ok()) {
      std::cerr << "Error looking up state: " << cmd.lastError() << std::endl;
      return false;
    } else {
      return true;
    }
  }
}

bool StateDB::delete_key(StateKey key) {
  if (!initialized_) {
    std::cout << "Cannot delete state from uninitialized StateDB" << std::endl;
    return false;
  } else {
    size_t hash = state_key_hash(key);
    redox::Command<int>& cmd =
        redis_connection_.commandSync<int>({"DEL", std::to_string(hash)});
    if (!cmd.ok()) {
      std::cerr << "Error deleting key: " << cmd.lastError() << std::endl;
      return false;
    } else {
      return true;
    }
  }
}

}  // namespace clipper
