
#include <cxxopts.hpp>

#include <clipper/app_metrics.hpp>
#include <clipper/config.hpp>
#include <clipper/constants.hpp>
#include <clipper/query_processor.hpp>

#include "query_frontend.hpp"

int main(int argc, char* argv[]) {
  cxxopts::Options options("query_frontend",
                           "Clipper query processing frontend");
  // clang-format off
  options.add_options()
    ("redis_ip", "Redis address",
        cxxopts::value<std::string>()->default_value("localhost"))
    ("redis_port", "Redis port",
        cxxopts::value<int>()->default_value("6379"))
    ("threadpool_size", "Number of threads for the task execution threadpool",
        cxxopts::value<int>()->default_value("4"));
  // clang-format on
  options.parse(argc, argv);

  clipper::Config& conf = clipper::get_config();
  conf.set_redis_address(options["redis_ip"].as<std::string>());
  conf.set_redis_port(options["redis_port"].as<int>());
  conf.set_task_execution_threadpool_size(options["threadpool_size"].as<int>());
  conf.ready();

  query_frontend::RequestHandler<clipper::QueryProcessor> rh(
      "0.0.0.0", clipper::QUERY_FRONTEND_PORT, 1);
  rh.start_listening();
}
