
#include <string>
#include <vector>

#include <clipper/datatypes.hpp>

namespace clipper {

Output::Output(double y_hat, std::string versioned_model)
    : y_hat_(y_hat), versioned_model_(versioned_model) {}

DoubleVector::DoubleVector(std::vector<double> data) : data_(std::move(data)) {}

ByteBuffer DoubleVector::serialize() const {
  std::vector<uint8_t> bytes;
  for (auto&& i : data_) {
    auto cur_bytes = reinterpret_cast<const uint8_t*>(&i);
    for (unsigned long j = 0; j < sizeof(double); ++j) {
      bytes.push_back(cur_bytes[j]);
    }
  }
  return bytes;
}

}  // namespace clipper
