#include <gtest/gtest.h>

#include <thallium.hpp>

#include "../provider.hpp"

#define ADDR "127.0.0.1:30000"
namespace {
class provider_test : public ::testing::Test {
protected:
  tl::engine server_engine;
  tl::engine client_engine;
  raft_provider provider;
  tl::remote_procedure m_echo_state_rpc;
  tl::provider_handle server_addr;
  provider_test()
    : server_engine("sockets://" ADDR, THALLIUM_SERVER_MODE, true, 2)
    , client_engine("sockets", THALLIUM_CLIENT_MODE)
    , provider(server_engine, RAFT_PROVIDER_ID)
    , m_echo_state_rpc(client_engine.define(ECHO_STATE_RPC_NAME))
    , server_addr(tl::provider_handle(client_engine.lookup("sockets://" ADDR),
                                      RAFT_PROVIDER_ID)) {
    server_engine.enable_remote_shutdown();
  }
  static void wait_loop(void *arg) { ((tl::engine *)arg)->finalize(); }
  ~provider_test() {
    ABT_xstream stream;
    ABT_thread thread;

    ABT_xstream_create(ABT_SCHED_NULL, &stream);
    ABT_thread_create_on_xstream(stream, wait_loop, &server_engine,
                                 ABT_THREAD_ATTR_NULL, &thread);
    cleanup();
  }
  void cleanup() {
    int err;
    std::string dir_path = "log-" ADDR;
    std::string data_path = dir_path + "/data.mdb";
    std::string lock_path = dir_path + "/lock.mdb";

    err = remove(data_path.c_str());
    if (err) { printf("remove %s error, %d\n", data_path.c_str(), err); }

    err = remove(lock_path.c_str());
    if (err) { printf("remove %s error, %d\n", lock_path.c_str(), err); }

    err = rmdir(dir_path.c_str());
    if (err) { printf("rmdir %s error %d\n", dir_path.c_str(), err); }
  }

  raft_state get_state() {
    int r = m_echo_state_rpc.on(server_addr)();
    return raft_state(r);
  }
};

TEST_F(provider_test, MUST_BE_FOLLOWER) {
  std::vector<std::string> nodes;
  provider.start(nodes);
  ASSERT_EQ(get_state(), raft_state::follower);
}

} // namespace