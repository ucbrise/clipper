
#include <atomic>
#include <chrono>
#include <iostream>
#include <sstream>
#include <thread>

#include <boost/thread.hpp>
#include <clipper/constants.hpp>
#include <clipper/persistent_state.hpp>

namespace clipper {

std::string generate_redis_key(const StateKey& key) {
  std::stringstream key_stream;
  key_stream << std::get<0>(key);
  key_stream << ":";
  key_stream << std::get<1>(key);
  key_stream << ":";
  key_stream << std::get<2>(key);
  return key_stream.str();
}

StateDB::StateDB() : initialized_(false) {
  std::cout << "Persistent state DB created" << std::endl;
}

bool StateDB::init() {
  Config& conf = get_config();
  while (!redis_connection_.connect(conf.get_redis_address(),
                                    conf.get_redis_port())) {
    std::cout << "ERROR connecting to Redis" << std::endl;
    std::cout << "Sleeping 1 second..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  redox::Command<std::string>& set_table_cmd =
      redis_connection_.commandSync<std::string>(
          {"SELECT", std::to_string(REDIS_STATE_DB_NUM)});
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
  std::string redis_key = generate_redis_key(key);
  redox::Command<std::string>& cmd =
      redis_connection_.commandSync<std::string>({"GET", redis_key});
  if (cmd.ok()) {
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
    std::string redis_key = generate_redis_key(key);
    redox::Command<std::string>& cmd =
        redis_connection_.commandSync<std::string>({"SET", redis_key, value});
    if (!cmd.ok()) {
      std::cerr << "Error modifying state: " << cmd.lastError() << std::endl;
      return false;
    } else {
      return true;
    }
  }
}

bool StateDB::remove(StateKey key) {
  if (!initialized_) {
    std::cout << "Cannot delete state from uninitialized StateDB" << std::endl;
    return false;
  } else {
    std::string redis_key = generate_redis_key(key);
    redox::Command<int>& cmd =
        redis_connection_.commandSync<int>({"DEL", redis_key});
    if (!cmd.ok()) {
      std::cerr << "Error deleting key: " << cmd.lastError() << std::endl;
      return false;
    } else {
      return true;
    }
  }
}

int StateDB::num_entries() {
  if (!initialized_) {
    std::cout << "Cannot count entries from uninitialized StateDB" << std::endl;
    return 0;
  }
  redox::Command<int>& cmd = redis_connection_.commandSync<int>({"DBSIZE"});
  if (cmd.ok()) {
    return cmd.reply();
  } else {
    std::cerr << "Error looking up state: " << cmd.lastError() << std::endl;
    return 0;
  }
}

}  // namespace clipper
