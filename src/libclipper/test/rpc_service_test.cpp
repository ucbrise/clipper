#include <memory>

#include <gtest/gtest.h>

#include <clipper/containers.hpp>
#include <clipper/rpc_service.hpp>

using namespace clipper;
using std::vector;

// Tests:
// + start (sending to non-started service)
// + stop (shuts down when shutdown signal given)
// + try_get_response() (never get more than max, get max if available, get all
// if less available)
// + manage service??

namespace {

class MockZmqSocket {};

TEST(RPCServiceTests, SendBeforeStart) {
  rpc::RPCService rpc{};
  EXPECT_EQ(-1, rpc.send_message(vector<ByteBuffer>(), 7));
}

}  // namespace

// int main(int argc, char**argv) {
//
//   ::testing::InitGoogleTest(&argc, argv);
//     return RUN_ALL_TESTS();
// }
