#include "provider.hpp"

#include <gtest/gtest.h>

#include <random>
#include <thallium.hpp>

#include "types.hpp"

#define ADDR "127.0.0.1:"

namespace {
class provider_test : public ::testing::Test {
protected:
  std::random_device rnd;
  int PORT;
  std::string addr, caddr;
  tl::abt scope;
  tl::engine server_engine;
  tl::engine client_engine;
  raft_provider provider;
  tl::remote_procedure m_echo_state_rpc;
  tl::remote_procedure m_request_vote_rpc;
  tl::remote_procedure m_append_entries_rpc;
  tl::remote_procedure m_client_put_rpc;
  tl::remote_procedure m_client_get_rpc;
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
    , m_client_put_rpc(client_engine.define(CLIENT_PUT_RPC_NAME))
    , m_client_get_rpc(client_engine.define(CLIENT_GET_RPC_NAME))
    , server_addr(
        tl::provider_handle(client_engine.lookup(addr), RAFT_PROVIDER_ID)) {
    std::cout << "server running at " << server_engine.self() << std::endl;
  }

  static void finalize(void *arg) { ((raft_provider *)arg)->finalize(); }

  ~provider_test() { cleanup(); }
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

  void TearDown() {
    server_addr = tl::provider_handle();
    m_echo_state_rpc.deregister();
    m_request_vote_rpc.deregister();
    m_append_entries_rpc.deregister();
    m_client_put_rpc.deregister();
    m_client_get_rpc.deregister();
    client_engine.finalize();

    ABT_xstream stream;
    ABT_thread thread;

    ABT_xstream_create(ABT_SCHED_NULL, &stream);
    ABT_thread_create_on_xstream(stream, finalize, &provider,
                                 ABT_THREAD_ATTR_NULL, &thread);
    server_engine.wait_for_finalize();

    ABT_thread_free(&thread);
    ABT_xstream_free(&stream);
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

  client_put_response client_put(std::string key, std::string value) {
    client_put_response resp = m_client_put_rpc.on(server_addr)(key, value);
    return resp;
  }

  client_get_response client_get(std::string key) {
    client_get_response resp = m_client_get_rpc.on(server_addr)(key);
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

TEST_F(provider_test, GET_RPC) {
  std::vector<std::string> nodes;
  provider.start(nodes);
  usleep(3 * INTERVAL);
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  client_get_response r = client_get("hello");
  ASSERT_EQ(r.get_error(), RAFT_SUCCESS);
  ASSERT_STREQ(r.get_value().c_str(), "world");
}

TEST_F(provider_test, PUT_RPC) {
  std::vector<std::string> nodes;
  provider.start(nodes);
  usleep(3 * INTERVAL);
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  client_put_response r = client_put("foo", "bar");
  ASSERT_EQ(r.get_error(), RAFT_SUCCESS);
  ASSERT_EQ(r.get_index(), 1);
  provider.run(); // commit "foo" "bar"
  provider.run(); // applied "foo" "bar"
  client_get_response r2 = client_get("foo");
  ASSERT_EQ(r2.get_error(), RAFT_SUCCESS);
  ASSERT_STREQ(r2.get_value().c_str(), "bar");
}

TEST_F(provider_test, PUT_RPC_LEADER_NOT_FOUND) {
  std::vector<std::string> nodes;
  provider.start(nodes);
  ASSERT_EQ(fetch_state(), raft_state::follower);
  client_put_response r = client_put("foo", "bar");
  ASSERT_EQ(r.get_error(), RAFT_LEADER_NOT_FOUND);
  ASSERT_EQ(r.get_index(), 0);
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

TEST_F(provider_test, NOT_FOUND_PREV_LOG) {
  std::vector<std::string> nodes;
  provider.start(nodes);
  ASSERT_EQ(fetch_state(), raft_state::follower);
  append_entries_response r =
    append_entries(2, 1, 1, std::vector<raft_entry>(), 0, caddr);
  ASSERT_EQ(r.get_term(), 2);
  ASSERT_FALSE(r.is_success());
  provider.run();
}

TEST_F(provider_test, CONFLICT_PREV_LOG) {
  std::vector<std::string> nodes;
  provider.start(nodes);
  usleep(3 * INTERVAL);
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  client_put_response r = client_put("foo", "bar");
  ASSERT_EQ(r.get_error(), RAFT_SUCCESS);
  ASSERT_EQ(r.get_index(), 1);
  append_entries_response r2 =
    append_entries(2, 1, 2, std::vector<raft_entry>(), 0, caddr);
  ASSERT_FALSE(r2.is_success());
  ASSERT_EQ(r2.get_term(), 2);
  ASSERT_EQ(fetch_state(), raft_state::follower);
}

TEST_F(provider_test, NOT_GRANTED_VOTE_WITH_LATE_LOG) {
  std::vector<std::string> nodes;
  provider.start(nodes);
  usleep(3 * INTERVAL);
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  client_put_response r = client_put("foo", "bar");
  ASSERT_EQ(r.get_error(), RAFT_SUCCESS);
  ASSERT_EQ(r.get_index(), 1);
  request_vote_response r2 = request_vote(2, caddr, 0, 0);
  ASSERT_FALSE(r2.is_vote_granted());
  ASSERT_EQ(r2.get_term(), 2);
  ASSERT_EQ(fetch_state(), raft_state::follower);
}

TEST_F(provider_test, NOT_GRANTED_VOTE_WITH_LATE_LOG_2) {
  std::vector<std::string> nodes;
  provider.start(nodes);
  usleep(3 * INTERVAL);
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  client_put_response r = client_put("foo", "bar");
  ASSERT_EQ(r.get_error(), RAFT_SUCCESS);
  ASSERT_EQ(r.get_index(), 1);
  request_vote_response r2 = request_vote(2, caddr, 0, 1);
  ASSERT_FALSE(r2.is_vote_granted());
  ASSERT_EQ(r2.get_term(), 2);
  ASSERT_EQ(fetch_state(), raft_state::follower);
}

TEST_F(provider_test, GRANTED_VOTE_WITH_LATEST_LOG) {
  std::vector<std::string> nodes;
  provider.start(nodes);
  request_vote_response r2 = request_vote(1, caddr, 1, 1);
  ASSERT_TRUE(r2.is_vote_granted());
  ASSERT_EQ(r2.get_term(), 1);
  ASSERT_EQ(fetch_state(), raft_state::follower);
}

TEST_F(provider_test, GRANTED_VOTE_WITH_LATEST_LOG_2) {
  std::vector<std::string> nodes;
  provider.start(nodes);
  usleep(3 * INTERVAL);
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  client_put_response r = client_put("foo", "bar");
  ASSERT_EQ(r.get_error(), RAFT_SUCCESS);
  ASSERT_EQ(r.get_index(), 1);
  request_vote_response r2 = request_vote(2, caddr, 1, 1);
  ASSERT_TRUE(r2.is_vote_granted());
  ASSERT_EQ(r2.get_term(), 2);
  ASSERT_EQ(fetch_state(), raft_state::follower);
}

TEST_F(provider_test, GRANTED_VOTE_WITH_LATEST_LOG_3) {
  std::vector<std::string> nodes;
  provider.start(nodes);
  usleep(3 * INTERVAL);
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  client_put_response r = client_put("foo", "bar");
  ASSERT_EQ(r.get_error(), RAFT_SUCCESS);
  ASSERT_EQ(r.get_index(), 1);
  request_vote_response r2 = request_vote(2, caddr, 0, 2);
  ASSERT_TRUE(r2.is_vote_granted());
  ASSERT_EQ(r2.get_term(), 2);
  ASSERT_EQ(fetch_state(), raft_state::follower);
}

TEST_F(provider_test, NOT_GRANTED_VOTE_WITH_ALREADY_VOTED) {
  std::vector<std::string> nodes;
  provider.start(nodes);
  usleep(3 * INTERVAL);
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  request_vote_response r2 = request_vote(1, caddr, 0, 0);
  ASSERT_FALSE(r2.is_vote_granted());
  ASSERT_EQ(r2.get_term(), 1);
  ASSERT_EQ(fetch_state(), raft_state::leader);
}

TEST_F(provider_test, APPLY_ENTRIES) {
  std::vector<std::string> nodes;
  provider.start(nodes);
  std::vector<raft_entry> ent;
  ent.emplace_back(1, "foo", "bar");
  append_entries_response r = append_entries(1, 0, 0, ent, 1, caddr);
  ASSERT_EQ(r.get_term(), 1);
  ASSERT_TRUE(r.is_success());
  usleep(3 * INTERVAL);
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  client_get_response r2 = client_get("foo");
  ASSERT_EQ(r2.get_error(), RAFT_SUCCESS);
  ASSERT_STREQ(r2.get_value().c_str(), "bar");
}

TEST_F(provider_test, NOT_DETERMINED_LEADER) {
  // dummy
  std::vector<std::string> nodes{"sockets://127.0.0.1:299999"};
  provider.start(nodes);
  usleep(3 * INTERVAL);
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::candidate);
}

TEST_F(provider_test, BECOME_FOLLOWER_FROM_CANDIDATE) {
  std::vector<std::string> nodes{"sockets://127.0.0.1:299999"};
  provider.start(nodes);
  usleep(3 * INTERVAL);
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::candidate);
  append_entries_response r =
    append_entries(1, 0, 0, std::vector<raft_entry>(), 0, caddr);
  ASSERT_TRUE(r.is_success());
  ASSERT_EQ(r.get_term(), 1);
  ASSERT_EQ(fetch_state(), raft_state::follower);
}

TEST_F(provider_test, CLIENT_GET_LEADER_NOT_FOUND) {
  std::vector<std::string> nodes{"sockets://127.0.0.1:299999"};
  provider.start(nodes);
  ASSERT_EQ(fetch_state(), raft_state::follower);
  client_get_response r = client_get("hello");
  ASSERT_EQ(r.get_error(), RAFT_LEADER_NOT_FOUND);
  ASSERT_STREQ(r.get_value().c_str(), "");
  usleep(3 * INTERVAL);
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::candidate);
  client_get_response r2 = client_get("hello");
  ASSERT_EQ(r2.get_error(), RAFT_LEADER_NOT_FOUND);
  ASSERT_STREQ(r2.get_value().c_str(), "");
}

} // namespace