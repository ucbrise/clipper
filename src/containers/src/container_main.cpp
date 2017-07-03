#include "container_rpc.hpp"

int main(int argc, char* argv[]) {
  clipper::container::RPC rpc;
  rpc.start("BLAH", 1);
}