#ifndef CLIPPER_LIB_DATATYPES_H
#define CLIPPER_LIB_DATATYPES_H

#include <string>
#include <vector>

namespace clipper {

using ByteBuffer = std::vector<uint8_t>;

class Output {
 public:
  ~Output() = default;

  Output(double y_hat, std::string versioned_model);
  double y_hat_;
  std::string versioned_model_;
};

// using Output = std::pair<double;

class Input {
 public:
  // TODO: pure virtual or default?
  // virtual ~Input() = default;

  // TODO special member functions:
  //    + explicit?
  //    + virtual?

  // used by RPC system
  virtual ByteBuffer serialize() const = 0;
};

// class IntVector : Input {
//   public:
//     IntVector(std::vector<int> data);
//
//     // move constructors
//     IntVector(IntVector&& other) = default;
//     IntVector& operator=(IntVector&& other) = default;
//
//     // copy constructors
//     IntVector(IntVector& other) = default;
//     IntVector& operator=(IntVector& other) = default;
//
//     ByteBuffer serialize() const;
//
//   private:
//     std::vector<int> data_;
// };

class DoubleVector : public Input {
 public:
  explicit DoubleVector(std::vector<double> data);

  // move constructors
  DoubleVector(DoubleVector&& other) = default;
  DoubleVector& operator=(DoubleVector&& other) = default;

  // copy constructors
  DoubleVector(DoubleVector& other) = default;
  DoubleVector& operator=(DoubleVector& other) = default;

  ByteBuffer serialize() const;

 private:
  std::vector<double> data_;
};

}  // namespace clipper

#endif  // CLIPPER_LIB_DATATYPES_H
