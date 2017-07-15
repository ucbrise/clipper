#include "r_models.hpp"

RNumericVectorModel::RNumericVectorModel() {

}

std::vector<std::string> RNumericVectorModel::predict(const std::vector<DoubleVector> inputs) const {

}

RIntegerVectorModel::RIntegerVectorModel() {

}

std::vector<std::string> RIntegerVectorModel::predict(const std::vector<IntVector> inputs) const {

}

RRawVectorModel::RRawVectorModel() {

}

std::vector<std::string> RRawVectorModel::predict(const std::vector<ByteVector> inputs) const {

}
