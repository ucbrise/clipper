
#include <cxxopts.hpp>

#include <clipper/query_processor.hpp>
#include "query_frontend.hpp"

int main(int argc, char* argv[]) {
  cxxopts::Options options("query_frontend",
                           "Clipper query processing frontend");
  // clang-format off
  options.add_options()
    ("redis_ip", "Redis address", cxxopts::value<std::string>()->default_value("localhost"))
    ("redis_port", "Redis port", cxxopts::value<int>()->default_value("6379"));
  // clang-format on
  options.parse(argc, argv);
  query_frontend::RequestHandler<clipper::QueryProcessor> rh("0.0.0.0", 1337,
                                                             1);
  rh.start_listening();
}
