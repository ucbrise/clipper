#include <stdexcept>

class json_parse_error : public std::runtime_error {
 public:
  json_parse_error(const std::string &what) : std::runtime_error(what) {}
  ~json_parse_error() throw() {};
};

class json_semantic_error : public std::runtime_error {
 public:
  json_semantic_error(const std::string &what) : std::runtime_error(what) {}
  ~json_semantic_error() throw() {};
};
