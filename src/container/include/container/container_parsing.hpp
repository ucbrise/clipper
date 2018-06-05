#ifndef CLIPPER_CONTAINER_PARSING_HPP
#define CLIPPER_CONTAINER_PARSING_HPP

#include <cstring>
#include <vector>

#include <container/datatypes.hpp>

namespace clipper {

namespace container {

template <typename T>
struct input_parser_type {
  static const bool is_string_parser = false;
};

template <>
struct input_parser_type<char> {
  static const bool is_string_parser = true;
};

/**
 * Parses raw data of type `D` to create `Input<D>` objects that will be
 * used by models to render predictions.
 *
 * @tparam D A primitive datatype corresponding to a valid Input<D> object
 * (ex: D == `uint8_t` is valid because ByteVector == Input<uint8_t> is a valid
 * Input object)
 */
template <typename D>
class InputBufferManager {
 public:
  InputBufferManager(std::vector<D>& buffer) : buffer_(buffer) {}
  /**
   * Exposes a buffer into which request data should be
   * read directly from a socket. The buffer contents
   * can then be parsed via `get_inputs`
   */
  std::vector<D>& get_data_buffer(long min_size_bytes) {
    resize_if_necessary(buffer_, min_size_bytes);
    return buffer_;
  }

 private:
  std::vector<D>& buffer_;

  void resize_if_necessary(std::vector<D>& buffer, long required_buffer_size) {
    if (static_cast<long>((buffer.size() * sizeof(D))) < required_buffer_size) {
      buffer.reserve((2 * required_buffer_size) / sizeof(D));
    }
  }
};

}  // namespace container

}  // namespace clipper

#endif  // CLIPPER_CONTAINER_PARSING_HPP
