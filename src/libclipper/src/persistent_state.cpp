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

StateCacheEntry::StateCacheEntry(const std::string& value)
    : entry_value_(value) {}

StateDB::StateDB()
    : cache_(std::unordered_map<StateKey, std::shared_ptr<StateCacheEntry>,
                                StateKeyHash, StateKeyEqual>(
          STATE_DB_CACHE_SIZE_ELEMENTS)) {
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
  auto entry_search = cache_.find(key);
  if (entry_search != cache_.end()) {
    auto& cache_entry = entry_search->second;
    std::lock_guard<std::mutex> entry_lock(cache_entry->entry_mtx_);
    return cache_entry->entry_value_;
  } else {
    std::string redis_key = generate_redis_key(key);
    const std::vector<std::string> cmd_vec{"GET", redis_key};
    auto result =
        redis::send_cmd_with_reply<std::string>(redis_connection_, cmd_vec);
    return result;
  }
}

bool StateDB::put(StateKey key, std::string value) {
  auto entry_search = cache_.find(key);
  bool new_entry = (entry_search == cache_.end());
  if (!new_entry) {
    std::lock_guard<std::mutex> lock(entry_search->second->entry_mtx_);
  }
  std::string redis_key = generate_redis_key(key);
  const std::vector<std::string> cmd_vec{"SET", redis_key, value};
  bool success =
      redis::send_cmd_no_reply<std::string>(redis_connection_, cmd_vec);
  if (success) {
    if (new_entry) {
      // This method is being invoked as part of the 'add application'
      // procedure in the query frontend. Because any subsequent feedback
      // updates to the StateDB depend on the completion of 'add application',
      // it's safe to add a new cache entry immediately
      std::shared_ptr<StateCacheEntry> entry =
          std::make_shared<StateCacheEntry>(value);
      cache_.emplace(key, entry);
    } else {
      // We already hold exclusive access to the preexisting entry, so
      // we can proceed to modify it
      entry_search->second->entry_value_ = value;
    }
  }
  return success;
}

bool StateDB::remove(StateKey key) {
  // We use a lock guard here because exclusive access
  // must be maintained until the entry is removed from the database
  std::string redis_key = generate_redis_key(key);
  const std::vector<std::string> cmd_vec{"DEL", redis_key};
  bool success = redis::send_cmd_no_reply<int>(redis_connection_, cmd_vec);
  if (success) {
    cache_.erase(key);
  }
  return success;
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
