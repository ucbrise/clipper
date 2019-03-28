#ifndef CLIPPER_LIB_CONSTANTS_HPP
#define CLIPPER_LIB_CONSTANTS_HPP

#include <string>
#include <utility>

namespace clipper {

enum RedisDBTable {
  REDIS_STATE_DB_NUM = 1,
  REDIS_MODEL_DB_NUM = 2,
  REDIS_CONTAINER_DB_NUM = 3,
  REDIS_RESOURCE_DB_NUM = 4,
  REDIS_APPLICATION_DB_NUM = 5,
  REDIS_METADATA_DB_NUM = 6,  // used to store Clipper configuration metadata
  REDIS_APP_MODEL_LINKS_DB_NUM = 7,
};

constexpr int QUERY_FRONTEND_PORT = 1337;
constexpr int MANAGEMENT_FRONTEND_PORT = 1338;

const std::string ITEM_DELIMITER = ",";

// used to concatenate multiple parts of an item, such as the
// name and version of a VersionedModelID
const std::string ITEM_PART_CONCATENATOR = ":";

const std::string LOGGING_TAG_CLIPPER = "Clipper";

constexpr int DEFAULT_USER_ID = 0;

}  // namespace clipper
#endif
