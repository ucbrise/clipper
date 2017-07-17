#include <iostream>

#include "r_models.hpp"

#include <Rcpp.h>
#include <container/datatypes.hpp>

RNumericVectorModel::RNumericVectorModel(const Rcpp::Function function) : function_(function) {

}

std::vector<std::string> RNumericVectorModel::predict(const std::vector<DoubleVector> inputs) const {
  std::vector<std::string> outputs;
  for(auto const& input : inputs) {
    Rcpp::NumericVector numeric_input(input.get_data(), input.get_data() + input.get_length());
    std::string output = Rcpp::as<std::string>(function_(numeric_input));
    outputs.push_back(std::move(output));
  }
  return outputs;
}

RIntegerVectorModel::RIntegerVectorModel(const Rcpp::Function function) : function_(function) {

}

std::vector<std::string> RIntegerVectorModel::predict(const std::vector<IntVector> inputs) const {
  std::vector<std::string> outputs;
  for(auto const& input : inputs) {
    Rcpp::IntegerVector integer_input(input.get_data(), input.get_data() + input.get_length());
    std::string output = Rcpp::as<std::string>(function_(integer_input));
    outputs.push_back(std::move(output));
  }
  return outputs;
}

RRawVectorModel::RRawVectorModel(const Rcpp::Function function) : function_(function) {

}

std::vector<std::string> RRawVectorModel::predict(const std::vector<ByteVector> inputs) const {
  std::vector<std::string> outputs;
  for(auto const& input : inputs) {
    Rcpp::RawVector raw_input(input.get_data(), input.get_data() + input.get_length());
    std::string output = Rcpp::as<std::string>(function_(raw_input));
    outputs.push_back(std::move(output));
  }
  return outputs;
}

RDataFrameModel::RDataFrameModel(const Rcpp::Function function) : function_(function) {

}

std::vector<std::string> RDataFrameModel::predict(const std::vector<SerializableString> inputs) const {
  std::vector<std::string> outputs;
  for(auto const& input : inputs) {
    Rcpp::CharacterVector serialized_dframe(input.get_data(), input.get_data() + input.get_length());
    std::string output = Rcpp::as<std::string>(function_(serialized_dframe));
    outputs.push_back(std::move(output));
  }
  return outputs;
}
