#include <atomic>
#include <chrono>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>


#include <clipper/config.hpp>
#include <clipper/constants.hpp>
#include <clipper/logging.hpp>
#include <clipper/persistent_state.hpp>
#include <clipper/redis.hpp>

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

StateDB::StateDB() {
  Config& conf = get_config();
  while (!redis_connection_.connect(conf.get_redis_address(),
                                    conf.get_redis_port())) {
    log_error(LOGGING_TAG_STATE_DB, "StateDB failed to connect to redis",
              "Retrying in 1 second...");
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  if (!redis::send_cmd_no_reply<std::string>(
          redis_connection_, {"SELECT", std::to_string(REDIS_STATE_DB_NUM)})) {
    throw std::runtime_error("Could not select StateDB table from Redis");
  }
  log_info(LOGGING_TAG_STATE_DB, "Persistent state DB created");
}

StateDB::~StateDB() { redis_connection_.disconnect(); }

boost::optional<std::string> StateDB::get(const StateKey& key) {
  std::string redis_key = generate_redis_key(key);
  const std::vector<std::string> cmd_vec{"GET", redis_key};
  return redis::send_cmd_with_reply<std::string>(redis_connection_, cmd_vec);
}

bool StateDB::put(StateKey key, std::string value) {
  std::string redis_key = generate_redis_key(key);
  const std::vector<std::string> cmd_vec{"SET", redis_key, value};
  return redis::send_cmd_no_reply<std::string>(redis_connection_, cmd_vec);
}

bool StateDB::remove(StateKey key) {
  std::string redis_key = generate_redis_key(key);
  const std::vector<std::string> cmd_vec{"DEL", redis_key};
  return redis::send_cmd_no_reply<int>(redis_connection_, cmd_vec);
}

int StateDB::num_entries() {
  const std::vector<std::string> cmd_vec{"DBSIZE"};
  boost::optional<int> result =
      redis::send_cmd_with_reply<int>(redis_connection_, cmd_vec);
  if (result) {
    return *result;
  } else {
    return 0;
  }
}

}  // namespace clipper
