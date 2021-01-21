#include "../provider.hpp"

#include <gtest/gtest.h>

#include <random>
#include <thallium.hpp>

#define ADDR "127.0.0.1:"

namespace {
class provider_test : public ::testing::Test {
protected:
  std::random_device rnd;
  int PORT;
  std::string addr, caddr;
  tl::engine server_engine;
  tl::engine client_engine;
  raft_provider provider;
  tl::remote_procedure m_echo_state_rpc;
  tl::remote_procedure m_request_vote_rpc;
  tl::remote_procedure m_append_entries_rpc;
  tl::provider_handle server_addr;
  provider_test()
    : PORT(rnd() % 1000 + 30000)
    , addr("sockets://" ADDR + std::to_string(PORT))
    , caddr("sockets://" ADDR + std::to_string(PORT + 1))
    , server_engine(addr, THALLIUM_SERVER_MODE, true, 2)
    , client_engine(caddr, THALLIUM_CLIENT_MODE)
    , provider(server_engine, RAFT_PROVIDER_ID)
    , m_echo_state_rpc(client_engine.define(ECHO_STATE_RPC_NAME))
    , m_request_vote_rpc(client_engine.define("request_vote"))
    , m_append_entries_rpc(client_engine.define("append_entries"))
    , server_addr(
        tl::provider_handle(client_engine.lookup(addr), RAFT_PROVIDER_ID)) {
    std::cout << "server running at " << server_engine.self() << std::endl;
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
    std::string dir_path = "log-" ADDR + std::to_string(PORT);
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

  request_vote_response request_vote(int term, std::string candidate_id,
                                     int last_log_index, int last_log_term) {
    request_vote_response resp = m_request_vote_rpc.on(server_addr)(
      term, candidate_id, last_log_index, last_log_term);
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
    append_entries(2, 0, 0, std::vector<raft_entry>(), 0, caddr);
  ASSERT_TRUE(r.is_success());
  ASSERT_EQ(r.get_term(), 2);
  ASSERT_EQ(fetch_state(), raft_state::follower);
}

TEST_F(provider_test, GET_HIGHER_TERM_2) {
  std::vector<std::string> nodes;
  provider.start(nodes);
  usleep(3 * INTERVAL);
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  request_vote_response r = request_vote(2, caddr, 0, 0);
  ASSERT_TRUE(r.is_vote_granted());
  ASSERT_EQ(r.get_term(), 2);
  ASSERT_EQ(fetch_state(), raft_state::follower);
}

TEST_F(provider_test, GET_LOWER_TERM) {
  std::vector<std::string> nodes;
  provider.start(nodes);
  usleep(3 * INTERVAL);
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  append_entries_response r =
    append_entries(0, 0, 0, std::vector<raft_entry>(), 0, caddr);
  ASSERT_FALSE(r.is_success());
  ASSERT_EQ(r.get_term(), 1);
  ASSERT_EQ(fetch_state(), raft_state::leader);
}

TEST_F(provider_test, GET_LOWER_TERM_2) {
  std::vector<std::string> nodes;
  provider.start(nodes);
  usleep(3 * INTERVAL);
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  request_vote_response r = request_vote(0, caddr, 0, 0);
  ASSERT_FALSE(r.is_vote_granted());
  ASSERT_EQ(r.get_term(), 1);
  ASSERT_EQ(fetch_state(), raft_state::leader);
}

} // namespace