#include <iostream>

#include "r_models.hpp"

#include <Rcpp.h>
#include "datatypes.hpp"

namespace container {

RNumericVectorModel::RNumericVectorModel(const Rcpp::Function function) : function_(function) {

}

std::vector<std::string> RNumericVectorModel::predict(const std::vector<DoubleVector> inputs) const {
  std::vector<std::string> outputs;
  std::vector<Rcpp::NumericVector> numeric_inputs;
  for(auto const& input : inputs) {
    Rcpp::NumericVector numeric_input(input.get_data(), input.get_data() + input.get_length());
    numeric_inputs.push_back(std::move(numeric_input));
  }
  Rcpp::List list = function_(Rcpp::wrap(numeric_inputs));
  for(Rcpp::List::iterator it = list.begin(); it != list.end(); ++it) {
    outputs.push_back(Rcpp::as<std::string>(*it));
  }
  return outputs;
}

RIntegerVectorModel::RIntegerVectorModel(const Rcpp::Function function) : function_(function) {

}

std::vector<std::string> RIntegerVectorModel::predict(const std::vector<IntVector> inputs) const {
  std::vector<std::string> outputs;
  std::vector<Rcpp::IntegerVector> integer_inputs;
  for(auto const& input : inputs) {
    Rcpp::IntegerVector integer_input(input.get_data(), input.get_data() + input.get_length());
    integer_inputs.push_back(std::move(integer_input));
  }
  Rcpp::List list = function_(Rcpp::wrap(integer_inputs));
  for(Rcpp::List::iterator it = list.begin(); it != list.end(); ++it) {
    outputs.push_back(Rcpp::as<std::string>(*it));
  }
  return outputs;
}

RRawVectorModel::RRawVectorModel(const Rcpp::Function function) : function_(function) {

}

std::vector<std::string> RRawVectorModel::predict(const std::vector<ByteVector> inputs) const {
  std::vector<std::string> outputs;
  std::vector<Rcpp::RawVector> raw_inputs;
  for(auto const& input : inputs) {
    Rcpp::RawVector raw_input(input.get_data(), input.get_data() + input.get_length());
    raw_inputs.push_back(std::move(raw_input));
  }
  Rcpp::List list = function_(Rcpp::wrap(raw_inputs));
  for(Rcpp::List::iterator it = list.begin(); it != list.end(); ++it) {
    outputs.push_back(Rcpp::as<std::string>(*it));
  }
  return outputs;
}

RCharacterVectorModel::RCharacterVectorModel(const Rcpp::Function function) : function_(function) {

}

std::vector<std::string> RCharacterVectorModel::predict(const std::vector<SerializableString> inputs) const {
  std::vector<std::string> outputs;
  std::vector<Rcpp::RawVector> raw_inputs;
  for(auto const& input : inputs) {
    Rcpp::RawVector raw_input(input.get_data(), input.get_data() + input.get_length());
    raw_inputs.push_back(std::move(raw_input));
  }
  Rcpp::List list = function_(Rcpp::wrap(raw_inputs));
  for(Rcpp::List::iterator it = list.begin(); it != list.end(); ++it) {
    outputs.push_back(Rcpp::as<std::string>(*it));
  }
  return outputs;
}

RSerializedInputModel::RSerializedInputModel(const Rcpp::Function function) : function_(function) {

}

std::vector<std::string> RSerializedInputModel::predict(const std::vector<SerializableString> inputs) const {
  std::vector<std::string> outputs;
  std::vector<Rcpp::RawVector> serialized_inputs;
  for(auto const& input : inputs) {
    Rcpp::RawVector serialized_input(input.get_data(), input.get_data() + input.get_length());
    serialized_inputs.push_back(std::move(serialized_input));
  }
  Rcpp::List list = function_(Rcpp::wrap(serialized_inputs));
  for(Rcpp::List::iterator it = list.begin(); it != list.end(); ++it) {
    outputs.push_back(Rcpp::as<std::string>(*it));
  }
  return outputs;
}

} // namespace container
