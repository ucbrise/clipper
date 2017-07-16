#include <iostream>

#include "r_models.hpp"

#include <Rcpp.h>
#include "datatypes.hpp"

namespace container {

RNumericVectorModel::RNumericVectorModel(Rcpp::Function function) : function_(function) {

}

std::vector<std::string> RNumericVectorModel::predict(const std::vector<DoubleVector> inputs) const {
  std::vector<std::string> outs;
  for(auto const& input : inputs) {
    Rcpp::NumericVector numeric_input(input.get_data(), input.get_data() + input.get_length());
    double result = Rcpp::as<double>(function_(numeric_input));
    outs.push_back(std::to_string(result));
  }
  return outs;
}

RIntegerVectorModel::RIntegerVectorModel() {

}

std::vector<std::string> RIntegerVectorModel::predict(const std::vector<IntVector> inputs) const {
  std::vector<std::string> outs;
  return outs;
}

RRawVectorModel::RRawVectorModel() {

}

std::vector<std::string> RRawVectorModel::predict(const std::vector<ByteVector> inputs) const {
  std::vector<std::string> outs;
  return outs;
}

RDataFrameModel::RDataFrameModel() {

}

std::vector<std::string> RDataFrameModel::predict(const std::vector<SerializableString> inputs) const {
  std::vector<std::string> outs;
  return outs;
}

} // namespace container
