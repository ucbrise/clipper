#include <container/container_rpc.hpp>
#include <container/container_parsing.hpp>

using namespace clipper::container;

int main(int argc, char* argv[]) {
  RPC container_rpc;
  RPCTestModel test_model(container_rpc);
  std::string model_name = "cpp_test";
  int model_version = 1;
  DoubleVectorParser parser;


  std::string clipper_ip = "localhost";
  int clipper_port = 7000;

  container_rpc.start(test_model, model_name, model_version, parser, clipper_ip, clipper_port);
  while(true);
}