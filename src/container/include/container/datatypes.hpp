#ifndef CLIPPER_DATATYPES_HPP
#define CLIPPER_DATATYPES_HPP

namespace clipper {

namespace container {

template <typename D>
class Input {
 public:
  Input(const D* data, size_t length) : data_(data), length_(length) {

  }

  const D* get_data() {
    return data_;
  }

  size_t get_length() {
    return length_;
  }

 private:
  D* data_;
  size_t length_;

};

typedef Input<uint8_t> ByteVector;
typedef Input<int> IntVector;
typedef Input<float> FloatVector;
typedef Input<double> DoubleVector;
typedef Input<char> SerializableString;

} // namespace container

} // namespace clipper

#endif //CLIPPER_DATATYPES_HPP
