#include <Rcpp.h>

#include <container/container_rpc.hpp>
#include "r_models.hpp"

using namespace container;
using namespace Rcpp;

// [[Rcpp::plugins(cpp11)]]

template <typename D>
void serve_model(SEXP& name, SEXP& version, SEXP& clipper_ip,
                 SEXP& clipper_port, Model<Input<D>>& model) {
  std::string parsed_name = Rcpp::as<std::string>(name);
  int parsed_version = Rcpp::as<int>(version);
  std::string parsed_clipper_ip = Rcpp::as<std::string>(clipper_ip);
  int parsed_clipper_port = Rcpp::as<int>(clipper_port);

  RPC rpc;
  rpc.start(model, parsed_name, parsed_version, parsed_clipper_ip,
            parsed_clipper_port);
}

RcppExport void serve_numeric_vector_model(SEXP name, SEXP version,
                                           SEXP clipper_ip, SEXP clipper_port,
                                           SEXP function) {
  Rcpp::Function predict_function(function);
  RNumericVectorModel model(predict_function);
  serve_model(name, version, clipper_ip, clipper_port, model);
}

RcppExport void serve_integer_vector_model(SEXP name, SEXP version,
                                           SEXP clipper_ip, SEXP clipper_port,
                                           SEXP function) {
  Rcpp::Function predict_function(function);
  RIntegerVectorModel model(predict_function);
  serve_model(name, version, clipper_ip, clipper_port, model);
}

RcppExport void serve_raw_vector_model(SEXP name, SEXP version, SEXP clipper_ip,
                                       SEXP clipper_port, SEXP function) {
  Rcpp::Function predict_function(function);
  RRawVectorModel model(predict_function);
  serve_model(name, version, clipper_ip, clipper_port, model);
}

RcppExport void serve_serialized_input_model(SEXP name, SEXP version,
                                             SEXP clipper_ip, SEXP clipper_port,
                                             SEXP function) {
  Rcpp::Function predict_function(function);
  RSerializedInputModel model(predict_function);
  serve_model(name, version, clipper_ip, clipper_port, model);
}