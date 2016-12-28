
#include "query_frontend.hpp"
#include <clipper/query_processor.hpp>

int main() {
  query_frontend::RequestHandler<clipper::QueryProcessor> rh("0.0.0.0", 1337,
                                                             1);
  std::vector<VersionedModelId> candidate_models{
      std::make_pair("m", 1), std::make_pair("j", 1),

  };
  rh.add_application("noop_app", candidate_models, InputType::Doubles,
                     query_frontend::OutputType::Double, "simple_policy",
                     20000);
  rh.start_listening();
}
