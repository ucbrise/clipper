#include <chrono>

#include <Rcpp.h>

#include "container_rpc.hpp"
#include "r_models.hpp"

using namespace container;
using namespace Rcpp;

// [[Rcpp::plugins(cpp11)]]

RcppExport void serve_numeric_vector_model(SEXP name, SEXP version, SEXP clipper_ip, SEXP clipper_port, SEXP function_name) {
	std::string parsed_function_name = Rcpp::as<std::string>(function_name);
	Rcpp::Environment env = Rcpp::Environment::global_env();
	Rcpp::Function predict_function = env[parsed_function_name];
	RNumericVectorModel model(predict_function);

	std::string parsed_name = Rcpp::as<std::string>(name);
	int parsed_version = Rcpp::as<int>(version);
	std::string parsed_clipper_ip = Rcpp::as<std::string>(clipper_ip);
	int parsed_clipper_port = Rcpp::as<int>(clipper_port);

	RPC rpc;
	rpc.start(model, parsed_name, parsed_version, parsed_clipper_ip, parsed_clipper_port);
}
