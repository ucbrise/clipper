
#include "management_frontend.hpp"

int main() {
  management::RequestHandler rh(1338, 1);
  rh.start_listening();
}
