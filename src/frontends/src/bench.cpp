#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#define BOOST_THREAD_VERSION 4
#include <boost/thread.hpp>
#include <boost/thread/future.hpp>

#include <clipper/datatypes.hpp>
#include <clipper/query_processor.hpp>

using namespace clipper;

int main() {
  QueryProcessor qp;
  std::shared_ptr<Input> input =
      std::make_shared<DoubleVector>(std::vector<double>{1.1, 2.2, 3.3, 4.4});
  auto prediction = qp.predict(
      {"test", 3, input, 20000, "newest_model", {std::make_pair("m", 1)}});
  std::cout << prediction.get().debug_string() << std::endl;
}
