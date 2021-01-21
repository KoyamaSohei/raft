#include "../provider.hpp"

#include <gtest/gtest.h>

#include <thallium.hpp>

#define ADDR "127.0.0.1:30000"
#define CADDR "127.0.0.1:30003"

namespace {
class provider_test : public ::testing::Test {
protected:
  tl::engine server_engine;
  tl::engine client_engine;
  raft_provider provider;
  tl::remote_procedure m_echo_state_rpc;
  tl::remote_procedure m_request_vote_rpc;
  tl::remote_procedure m_append_entries_rpc;
  tl::provider_handle server_addr;
  provider_test()
    : server_engine("sockets://" ADDR, THALLIUM_SERVER_MODE, true, 2)
    , client_engine("sockets://" CADDR, THALLIUM_CLIENT_MODE)
    , provider(server_engine, RAFT_PROVIDER_ID)
    , m_echo_state_rpc(client_engine.define(ECHO_STATE_RPC_NAME))
    , m_request_vote_rpc(client_engine.define("request_vote"))
    , m_append_entries_rpc(client_engine.define("append_entries"))
    , server_addr(tl::provider_handle(client_engine.lookup("sockets://" ADDR),
                                      RAFT_PROVIDER_ID)) {
    server_engine.enable_remote_shutdown();
  }

  static void finalize(void *arg) { ((tl::engine *)arg)->finalize(); }

  ~provider_test() {
    ABT_xstream stream;
    ABT_thread thread;

    ABT_xstream_create(ABT_SCHED_NULL, &stream);
    ABT_thread_create_on_xstream(stream, finalize, &server_engine,
                                 ABT_THREAD_ATTR_NULL, &thread);
    cleanup();
  }
  void cleanup() {
    int err;
    std::string dir_path = "log-" ADDR;
    std::string data_path = dir_path + "/data.mdb";
    std::string lock_path = dir_path + "/lock.mdb";

    err = remove(data_path.c_str());
    ASSERT_EQ(err, 0);

    err = remove(lock_path.c_str());
    ASSERT_EQ(err, 0);

    err = rmdir(dir_path.c_str());
    ASSERT_EQ(err, 0);
  }

  raft_state fetch_state() {
    int r = m_echo_state_rpc.on(server_addr)();
    return raft_state(r);
  }

  append_entries_response append_entries(int term, int prev_index,
                                         int prev_term,
                                         std::vector<raft_entry> entries,
                                         int leader_commit,
                                         std::string leader_id) {
    append_entries_response resp = m_append_entries_rpc.on(server_addr)(
      term, prev_index, prev_term, entries, leader_commit, leader_id);
    return resp;
  }
};

TEST_F(provider_test, BECOME_FOLLOWER) {
  std::vector<std::string> nodes;
  provider.start(nodes);
  ASSERT_EQ(fetch_state(), raft_state::follower);
}

TEST_F(provider_test, BECOME_LEADER) {
  std::vector<std::string> nodes;
  provider.start(nodes);
  usleep(3 * INTERVAL);
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
}

TEST_F(provider_test, GET_HIGHER_TERM) {
  std::vector<std::string> nodes;
  provider.start(nodes);
  usleep(3 * INTERVAL);
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  append_entries_response r =
    append_entries(2, 0, 0, std::vector<raft_entry>(), 0, "sockets://" CADDR);
  ASSERT_TRUE(r.is_success());
  ASSERT_EQ(r.get_term(), 2);
  ASSERT_EQ(fetch_state(), raft_state::follower);
}

} // namespace