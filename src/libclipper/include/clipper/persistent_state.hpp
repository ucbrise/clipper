#ifndef CLIPPER_LIB_PERSISTENT_STATE_HPP
#define CLIPPER_LIB_PERSISTENT_STATE_HPP

#include <atomic>
#include <chrono>
#include <functional>
#include <shared_mutex>
#include <tuple>
#include <unordered_map>

#include <boost/optional.hpp>
#include <redox.hpp>

#include "constants.hpp"
#include "datatypes.hpp"

namespace clipper {

const std::string LOGGING_TAG_STATE_DB = "STATEDB";
constexpr size_t STATE_DB_CACHE_SIZE_ELEMENTS = 1024;

// The entries in the key are query_label, user_id, model_hash
using StateKey = std::tuple<std::string, long, long>;

class StateKeyHash {
 public:
  size_t operator()(const StateKey& key) const {
    std::string str = std::get<0>(key);
    size_t hash = boost::hash_range(str.begin(), str.end());
    boost::hash_combine(hash, std::get<1>(key));
    boost::hash_combine(hash, std::get<2>(key));
    return hash;
  }
};
class StateKeyEqual {
 public:
  bool operator()(StateKey const& t1, StateKey const& t2) const {
    return (std::get<0>(t1) == std::get<0>(t2)) &&
           (std::get<1>(t1) == std::get<1>(t2)) &&
           (std::get<2>(t1) == std::get<2>(t2));
  }
};

class StateCacheEntry {
 public:
  explicit StateCacheEntry(const std::string& value);
  explicit StateCacheEntry();

  std::mutex mtx_;
  std::condition_variable cv_;
  boost::optional<std::string> value_;
  std::atomic<uint32_t> num_writers_;
  std::chrono::time_point<std::chrono::system_clock> last_written_;
};

// Threadsafe, non-copyable state storage
class StateDB {
 public:
  StateDB();
  ~StateDB();

  // Disallow copies because of the mutex
  StateDB(const StateDB&) = delete;
  StateDB& operator=(const StateDB&) = delete;

  StateDB(StateDB&&) = default;

  StateDB& operator=(StateDB&&) = default;

  /**
   * Get the value associated with the key if present
   * in the DB.
   *
   * @return Returns boost::none if the key is not found.
   */
  boost::optional<std::string> get(const StateKey& key);

  /**
   * Puts the key-value pair into the DB. If the key already
   * exists in the DB, the new value will blindly overwrite the old
   * value.
   *
   * @return Logs an error and returns false if there was an unexpected
   * error with the put.
   */
  bool put(StateKey key, std::string value);

  /**
   * Removes the entry associated with the key from the DB if present.
   * If the key is not present in the DB, this method has no effect.
   *
   * @return Logs an error and returns false if there was an unexpected
   * error with the removal.
   */
  bool remove(StateKey key);

  /**
   * Returns the total number of keys in the DB.
   */
  int num_entries();

 private:
  redox::Redox redis_connection_;
  std::unordered_map<StateKey, std::shared_ptr<StateCacheEntry>, StateKeyHash,
                     StateKeyEqual>
      cache_;
  std::mutex cache_mtx_;
};

}  // namespace clipper

#endif  // CLIPPER_LIB_PERSISTENT_STATE_HPP
