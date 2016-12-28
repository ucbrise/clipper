#ifndef CLIPPER_LIB_CONSTANTS_HPP
#define CLIPPER_LIB_CONSTANTS_HPP

#include <string>

namespace clipper {

const std::string REDIS_ADDRESS = "localhost";
constexpr int REDIS_PORT = 6379;
constexpr int REDIS_STATE_DB_NUM = 1;
constexpr int REDIS_MODEL_DB_NUM = 2;
constexpr int REDIS_CONTAINER_DB_NUM = 3;
constexpr int REDIS_RESOURCE_DB_NUM = 4;

constexpr int RPC_SERVICE_PORT = 7000;

}  // namespace clipper

#endif
