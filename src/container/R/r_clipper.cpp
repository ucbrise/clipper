#include <Rcpp.h>

#include <container_rpc.hpp>
#include <r_models.hpp>

using namespace container;
using namespace Rcpp;

// [[Rcpp::plugins(cpp11)]]

RcppExport void serve_numeric_vector_model(std::string name,
                                int version,
                                std::string clipper_ip,
                                int clipper_port,
                                Rcpp::Function function) {
  RNumericVectorModel model(function);
  RPC rpc;
  rpc.start(model, name, version, clipper_ip, clipper_port);
}