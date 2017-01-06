#ifndef CLIPPER_LIB_CONSTANTS_HPP
#define CLIPPER_LIB_CONSTANTS_HPP

#include <utility>

namespace clipper {

enum RedisDBTable {
  REDIS_STATE_DB_NUM = 1,
  REDIS_MODEL_DB_NUM = 2,
  REDIS_CONTAINER_DB_NUM = 3,
  REDIS_RESOURCE_DB_NUM = 4,
  REDIS_APPLICATION_DB_NUM = 5,
};

constexpr int RPC_SERVICE_PORT = 7000;

constexpr int QUERY_FRONTEND_PORT = 1337;
constexpr int MANAGEMENT_FRONTEND_PORT = 1338;

}  // namespace clipper
#endif
