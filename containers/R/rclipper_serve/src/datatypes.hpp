/**
  NOTE: This file is a thinner version of `datatypes.hpp/cpp`
  within the libclipper source. We use this thin copy
  to decouple the R container from the Clipper C++ libraries.
*/

#ifndef CLIPPER_DATATYPES_HPP
#define CLIPPER_DATATYPES_HPP

namespace container {

enum class RPCEvent {
  SentHeartbeat = 1,
  ReceivedHeartbeat = 2,
  SentContainerMetadata = 3,
  ReceivedContainerMetadata = 4,
  SentContainerContent = 5,
  ReceivedContainerContent = 6
};

enum class MessageType {
  NewContainer = 0,
  ContainerContent = 1,
  Heartbeat = 2
};

enum class HeartbeatType { KeepAlive = 0, RequestContainerMetadata = 1 };

enum class InputType {
  Invalid = -1,
  Bytes = 0,
  Ints = 1,
  Floats = 2,
  Doubles = 3,
  Strings = 4,
};

enum class RequestType {
  PredictRequest = 0,
  FeedbackRequest = 1,
};

static inline std::string get_readable_input_type(InputType type) {
  switch (type) {
    case InputType::Bytes: return std::string("bytes");
    case InputType::Ints: return std::string("integers");
    case InputType::Floats: return std::string("floats");
    case InputType::Doubles: return std::string("doubles");
    case InputType::Strings: return std::string("strings");
    case InputType::Invalid:
    default:
      return std::string("Invalid input type");
  }
}

template <typename D>
class Input {
 public:
  Input(const D* data, size_t length_bytes) : data_(data), length_(length_bytes / sizeof(D)) {}

  const D* get_data() const { return data_; }

  size_t get_length() const { return length_; }

 private:
  const D* data_;
  size_t length_;
};

typedef Input<uint8_t> ByteVector;
typedef Input<int> IntVector;
typedef Input<float> FloatVector;
typedef Input<double> DoubleVector;
typedef Input<char> SerializableString;

}  // namespace container

#endif  // CLIPPER_DATATYPES_HPP
