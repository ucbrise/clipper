#include <clipper/config.hpp>
#include <clipper/constants.hpp>
#include <clipper/query_processor.hpp>
#include <cxxopts.hpp>

#include "query_frontend.hpp"

int main(int argc, char* argv[]) {
  cxxopts::Options options("query_frontend",
                           "Clipper query processing frontend");
  // clang-format off
  options.add_options()
    ("redis_ip", "Redis address",
        cxxopts::value<std::string>()->default_value(clipper::DEFAULT_REDIS_ADDRESS))
    ("redis_port", "Redis port",
        cxxopts::value<int>()->default_value(std::to_string(clipper::DEFAULT_REDIS_PORT)))
    ("rpc_service_port", "RPCService's port",
        cxxopts::value<int>()->default_value(std::to_string(clipper::DEFAULT_RPC_SERVICE_PORT)))
    ("prediction_cache_size", "Size of the prediction cache in bytes, excluding cache metadata",
        cxxopts::value<long>()->default_value(std::to_string(clipper::DEFAULT_PREDICTION_CACHE_SIZE_BYTES)))
    ("thread_pool_size", "thread pool size of server_http",
        cxxopts::value<int>()->default_value(std::to_string(clipper::DEFAULT_THREAD_POOL_SIZE)))
    ("timeout_request", "timeout_request of server_http",
        cxxopts::value<int>()->default_value(std::to_string(clipper::DEFAULT_TIMEOUT_REQUEST)))
    ("timeout_content", "timeout_content of server_http",
        cxxopts::value<int>()->default_value(std::to_string(clipper::DEFAULT_TIMEOUT_CONTENT)));
  // clang-format on
  options.parse(argc, argv);

  clipper::Config& conf = clipper::get_config();
  conf.set_redis_address(options["redis_ip"].as<std::string>());
  conf.set_redis_port(options["redis_port"].as<int>());
  conf.set_rpc_service_port(options["rpc_service_port"].as<int>());
  conf.set_prediction_cache_size(options["prediction_cache_size"].as<long>());
  conf.ready();

  query_frontend::RequestHandler<clipper::QueryProcessor> rh(
      "0.0.0.0", clipper::QUERY_FRONTEND_PORT,
      options["thread_pool_size"].as<int>(),
      options["timeout_request"].as<int>(),
      options["timeout_content"].as<int>());
  rh.start_listening();
}
