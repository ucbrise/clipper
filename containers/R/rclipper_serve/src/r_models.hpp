#ifndef CLIPPER_R_MODELS_HPP
#define CLIPPER_R_MODELS_HPP

#include <Rcpp.h>

#include "container_rpc.hpp"
#include "datatypes.hpp"

namespace container {

class RNumericVectorModel : public Model<DoubleVector> {
 public:
  RNumericVectorModel(const Rcpp::Function function);
  std::vector<std::string> predict(const std::vector<DoubleVector> inputs) const override;

 private:
  const Rcpp::Function function_;

};

class RIntegerVectorModel : public Model<IntVector> {
 public:
  RIntegerVectorModel(const Rcpp::Function function);
  std::vector<std::string> predict(const std::vector<IntVector> inputs) const override;

 private:
  const Rcpp::Function function_;
};

class RRawVectorModel : public Model<ByteVector> {
 public:
  RRawVectorModel(const Rcpp::Function function);
  std::vector<std::string> predict(const std::vector<ByteVector> inputs) const override;

 private:
  const Rcpp::Function function_;
};

class RCharacterVectorModel : public Model<SerializableString> {
 public:
  RCharacterVectorModel(const Rcpp::Function function);
  std::vector<std::string> predict(const std::vector<SerializableString> inputs) const override;

 private:
  const Rcpp::Function function_;
};

class RSerializedInputModel : public Model<SerializableString> {
 public:
  RSerializedInputModel(const Rcpp::Function function);
  std::vector<std::string> predict(const std::vector<SerializableString> inputs) const override;

 private:
  const Rcpp::Function function_;
};

} // namespace container

#endif //CLIPPER_R_MODELS_HPP
