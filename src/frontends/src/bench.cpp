#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <boost/thread.hpp>

#include <clipper/constants.hpp>
#include <clipper/datatypes.hpp>
#include <clipper/logging.hpp>
#include <clipper/query_processor.hpp>

using namespace clipper;

int main() {
  QueryProcessor qp;
  std::shared_ptr<Input> input =
      std::make_shared<DoubleVector>(std::vector<double>{1.1, 2.2, 3.3, 4.4});
  boost::future<Response> prediction =
      qp.predict({"test", 3, input, 20000, "EXP3", {std::make_pair("m", 1)}});
  log_info(LOGGING_TAG_CLIPPER, prediction.get().debug_string());
}
