#ifndef CLIPPER_R_MODELS_HPP
#define CLIPPER_R_MODELS_HPP

#include <container/container_rpc.hpp>
#include <container/datatypes.hpp>

using namespace container;

class RNumericVectorModel : public Model<DoubleVector> {
 public:
  RNumericVectorModel();
  std::vector<std::string> predict(const std::vector<DoubleVector> inputs) const override;
  InputType get_input_type() const override;

};

class RIntegerVectorModel : public Model<IntVector> {
 public:
  RIntegerVectorModel();
  std::vector<std::string> predict(const std::vector<IntVector> inputs) const override;
  InputType get_input_type() const override;
};

class RRawVectorModel : public Model<ByteVector> {
 public:
  RRawVectorModel();
  std::vector<std::string> predict(const std::vector<ByteVector> inputs) const override;
  InputType get_input_type() const override;
};

class RDataFrameModel : public Model<SerializableString> {
 public:
  RDataFrameModel();
  std::vector<std::string> predict(const std::vector<SerializableString> inputs) const override;
  InputType get_input_type() const override;
};

#endif //CLIPPER_R_MODELS_HPP
