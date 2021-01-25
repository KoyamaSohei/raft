#include "provider.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <random>
#include <thallium.hpp>

#include "fsm.hpp"
#include "logger.hpp"
#include "types.hpp"

#define ADDR "127.0.0.1:"

namespace {

using ::testing::_;
using ::testing::Invoke;

class mock_raft_fsm : public raft_fsm {
private:
  kvs_raft_fsm real_;

public:
  mock_raft_fsm() {
    ON_CALL(*this, apply(_))
      .WillByDefault(Invoke(&real_, &kvs_raft_fsm::apply));
    ON_CALL(*this, resolve(_))
      .WillByDefault(Invoke(&real_, &kvs_raft_fsm::resolve));
  }
  MOCK_METHOD1(apply, void(std::string));
  MOCK_METHOD1(resolve, std::string(std::string));
};

class mock_raft_logger : public raft_logger {
private:
  lmdb_raft_logger real_;

public:
  mock_raft_logger(std::string _id, std::set<std::string> nodes)
    : raft_logger(_id, nodes), real_(_id, nodes) {
    ON_CALL(*this, init())
      .WillByDefault(Invoke(&real_, &lmdb_raft_logger::init));
    ON_CALL(*this, clean_up())
      .WillByDefault(Invoke(&real_, &lmdb_raft_logger::clean_up));
    ON_CALL(*this, get_id())
      .WillByDefault(Invoke(&real_, &lmdb_raft_logger::get_id));
    ON_CALL(*this, get_peers())
      .WillByDefault(Invoke(&real_, &lmdb_raft_logger::get_peers));
    ON_CALL(*this, get_num_nodes())
      .WillByDefault(Invoke(&real_, &lmdb_raft_logger::get_num_nodes));
    ON_CALL(*this, get_current_term())
      .WillByDefault(Invoke(&real_, &lmdb_raft_logger::get_current_term));
    ON_CALL(*this, set_current_term(_))
      .WillByDefault(Invoke(&real_, &lmdb_raft_logger::set_current_term));
    ON_CALL(*this, exists_voted_for)
      .WillByDefault(Invoke(&real_, &lmdb_raft_logger::exists_voted_for));
    ON_CALL(*this, clear_voted_for)
      .WillByDefault(Invoke(&real_, &lmdb_raft_logger::clear_voted_for));
    ON_CALL(*this, set_voted_for_self)
      .WillByDefault(Invoke(&real_, &lmdb_raft_logger::set_voted_for_self));
    ON_CALL(*this, set_voted_for)
      .WillByDefault(Invoke(&real_, &lmdb_raft_logger::set_voted_for));
    ON_CALL(*this, get_log(_, _, _, _))
      .WillByDefault(Invoke(&real_, &lmdb_raft_logger::get_log));
    ON_CALL(*this, set_log(_, _, _, _))
      .WillByDefault(Invoke(&real_, &lmdb_raft_logger::set_log));
    ON_CALL(*this, append_log(_, _))
      .WillByDefault(Invoke(&real_, &lmdb_raft_logger::append_log));
    ON_CALL(*this, get_term(_))
      .WillByDefault(Invoke(&real_, &lmdb_raft_logger::get_term));
    ON_CALL(*this, get_last_log(_, _))
      .WillByDefault(Invoke(&real_, &lmdb_raft_logger::get_last_log));
    ON_CALL(*this, match_log(_, _))
      .WillByDefault(Invoke(&real_, &lmdb_raft_logger::match_log));
    ON_CALL(*this, contains_uuid(_))
      .WillByDefault(Invoke(&real_, &lmdb_raft_logger::contains_uuid));
    ON_CALL(*this, get_last_conf_applied())
      .WillByDefault(Invoke(&real_, &lmdb_raft_logger::get_last_conf_applied));
    ON_CALL(*this, set_add_conf_log(_, _, _))
      .WillByDefault(Invoke(&real_, &lmdb_raft_logger::set_add_conf_log));
    ON_CALL(*this, set_remove_conf_log(_, _, _))
      .WillByDefault(Invoke(&real_, &lmdb_raft_logger::set_remove_conf_log));
  }
  ~mock_raft_logger() { real_.clean_up(); }
  MOCK_METHOD0(init, void());
  MOCK_METHOD0(clean_up, void());
  MOCK_METHOD0(get_id, std::string());
  MOCK_METHOD0(get_peers, std::set<std::string> &());
  MOCK_METHOD0(get_num_nodes, int());
  MOCK_METHOD0(get_current_term, int());
  MOCK_METHOD1(set_current_term, void(int));
  MOCK_METHOD0(exists_voted_for, bool());
  MOCK_METHOD0(clear_voted_for, void());
  MOCK_METHOD0(set_voted_for_self, void());
  MOCK_METHOD1(set_voted_for, void(const std::string &new_addr));
  MOCK_METHOD4(get_log, void(const int index, int &term, std::string &uuid,
                             std::string &command));
  MOCK_METHOD4(set_log,
               void(const int index, const int term, const std::string &uuid,
                    const std::string &command));
  MOCK_METHOD2(append_log,
               int(const std::string &uuid, const std::string &command));
  MOCK_METHOD1(get_term, int(const int index));
  MOCK_METHOD2(get_last_log, void(int &index, int &term));
  MOCK_METHOD2(match_log, bool(const int index, const int term));
  MOCK_METHOD1(contains_uuid, bool(const std::string &uuid));
  MOCK_METHOD0(get_last_conf_applied, int());
  MOCK_METHOD3(set_add_conf_log, void(const int &term, const std::string &uuid,
                                      const std::string &new_server));
  MOCK_METHOD3(set_remove_conf_log,
               void(const int &term, const std::string &uuid,
                    const std::string &old_server));
};

class provider_test : public ::testing::Test {
protected:
  std::random_device rnd;
  int PORT;
  std::string addr, caddr;
  tl::abt scope;
  tl::engine server_engine;
  tl::engine client_engine;
  mock_raft_logger *logger;
  mock_raft_fsm *fsm;
  raft_provider *provider;
  tl::remote_procedure m_echo_state_rpc;
  tl::remote_procedure m_request_vote_rpc;
  tl::remote_procedure m_append_entries_rpc;
  tl::remote_procedure m_timeout_now_rpc;
  tl::remote_procedure m_client_request_rpc;
  tl::remote_procedure m_client_query_rpc;
  tl::provider_handle server_addr;
  provider_test()
    : PORT(rnd() % 1000 + 30000)
    , addr(ADDR + std::to_string(PORT))
    , caddr(ADDR + std::to_string(PORT + 1))
    , server_engine(PROTOCOL_PREFIX + addr, THALLIUM_SERVER_MODE, true, 2)
    , client_engine(PROTOCOL_PREFIX + caddr, THALLIUM_CLIENT_MODE)
    , m_echo_state_rpc(client_engine.define(ECHO_STATE_RPC_NAME))
    , m_request_vote_rpc(client_engine.define("request_vote"))
    , m_append_entries_rpc(client_engine.define("append_entries"))
    , m_timeout_now_rpc(client_engine.define("timeout_now"))
    , m_client_request_rpc(client_engine.define(CLIENT_REQUEST_RPC_NAME))
    , m_client_query_rpc(client_engine.define(CLIENT_QUERY_RPC_NAME))
    , server_addr(tl::provider_handle(
        client_engine.lookup(PROTOCOL_PREFIX + addr), RAFT_PROVIDER_ID)) {
    std::cout << "server running at " << server_engine.self() << std::endl;
  }

  void SetUp(std::set<std::string> nodes) {
    logger = new mock_raft_logger(addr, nodes);
    fsm = new mock_raft_fsm();
    logger->init();
    provider = new raft_provider(server_engine, logger, fsm);
    provider->start();
  }

  ~provider_test() {
    delete provider;
    delete logger;
    delete fsm;
  }
  void TearDown() {
    server_addr = tl::provider_handle();
    m_echo_state_rpc.deregister();
    m_request_vote_rpc.deregister();
    m_append_entries_rpc.deregister();
    m_timeout_now_rpc.deregister();
    m_client_request_rpc.deregister();
    m_client_query_rpc.deregister();
    client_engine.finalize();
    server_engine.finalize();
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

  int timeout_now(int term, int prev_index, int prev_term) {
    int err = m_timeout_now_rpc.on(server_addr)(term, prev_index, prev_term);
    return err;
  }

  client_request_response client_request(std::string uuid,
                                         std::string command) {
    tl::async_response req =
      m_client_request_rpc.on(server_addr).async(uuid, command);

    usleep(INTERVAL);
    // to commit log in run_leader
    provider->run();
    provider->run();

    client_request_response resp = req.wait();
    return resp;
  }

  client_query_response client_query(std::string query) {
    client_query_response resp = m_client_query_rpc.on(server_addr)(query);
    return resp;
  }
};

TEST_F(provider_test, BECOME_FOLLOWER) {
  SetUp(std::set<std::string>{addr});
  ASSERT_EQ(fetch_state(), raft_state::follower);
}

TEST_F(provider_test, BECOME_LEADER) {
  SetUp(std::set<std::string>{addr});
  usleep(3 * INTERVAL);
  EXPECT_CALL(*logger, set_voted_for_self());
  EXPECT_CALL(*logger, set_current_term(1));
  provider->run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
}

TEST_F(provider_test, QUERY_RPC) {
  SetUp(std::set<std::string>{addr});
  usleep(3 * INTERVAL);
  EXPECT_CALL(*logger, set_voted_for_self());
  EXPECT_CALL(*logger, set_current_term(1));
  provider->run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  client_query_response r = client_query("hello");
  ASSERT_EQ(r.get_status(), RAFT_SUCCESS);
  ASSERT_STREQ(r.get_response().c_str(), "world");
}

TEST_F(provider_test, REQUEST_RPC) {
  SetUp(std::set<std::string>{addr});
  usleep(3 * INTERVAL);
  EXPECT_CALL(*logger, set_voted_for_self());
  EXPECT_CALL(*logger, set_current_term(1));
  provider->run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  EXPECT_CALL(*logger, contains_uuid("046ccc3a-2dac-4e40-ae2e-76797a271fe2"));
  EXPECT_CALL(*logger, append_log("046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                                  "{\"key\":\"foo\",\"value\":\"bar\"}"));
  client_request_response r =
    client_request("046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                   "{\"key\":\"foo\",\"value\":\"bar\"}");
  ASSERT_EQ(r.get_status(), RAFT_SUCCESS);
  ASSERT_EQ(r.get_index(), 2);
  provider->run(); // applied "foo" "bar"
  client_query_response r2 = client_query("foo");
  ASSERT_EQ(r2.get_status(), RAFT_SUCCESS);
  ASSERT_STREQ(r2.get_response().c_str(), "bar");
}

TEST_F(provider_test, REQUEST_RPC_LEADER_NOT_FOUND) {
  SetUp(std::set<std::string>{addr});
  ASSERT_EQ(fetch_state(), raft_state::follower);
  client_request_response r =
    client_request("046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                   "{\"key\":\"foo\",\"value\":\"bar\"}");
  ASSERT_EQ(r.get_status(), RAFT_LEADER_NOT_FOUND);
  ASSERT_EQ(r.get_index(), 0);
}

TEST_F(provider_test, GET_HIGHER_TERM) {
  SetUp(std::set<std::string>{addr});
  usleep(3 * INTERVAL);
  EXPECT_CALL(*logger, set_voted_for_self());
  EXPECT_CALL(*logger, set_current_term(1));
  provider->run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  EXPECT_CALL(*logger, set_current_term(2));
  append_entries_response r =
    append_entries(2, 0, 0, std::vector<raft_entry>(), 0, caddr);
  ASSERT_TRUE(r.is_success());
  ASSERT_EQ(r.get_term(), 2);
  ASSERT_EQ(fetch_state(), raft_state::follower);
}

TEST_F(provider_test, GET_HIGHER_TERM_2) {
  SetUp(std::set<std::string>{addr});
  usleep(3 * INTERVAL);
  EXPECT_CALL(*logger, set_voted_for_self());
  EXPECT_CALL(*logger, set_current_term(1));
  provider->run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  EXPECT_CALL(*logger, set_voted_for(caddr));
  EXPECT_CALL(*logger, set_current_term(2));
  request_vote_response r = request_vote(2, caddr, 1, 1);
  ASSERT_TRUE(r.is_vote_granted());
  ASSERT_EQ(r.get_term(), 2);
  ASSERT_EQ(fetch_state(), raft_state::follower);
}

TEST_F(provider_test, GET_LOWER_TERM) {
  SetUp(std::set<std::string>{addr});
  usleep(3 * INTERVAL);
  EXPECT_CALL(*logger, set_voted_for_self());
  EXPECT_CALL(*logger, set_current_term(1));
  provider->run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  EXPECT_CALL(*logger, set_voted_for(_)).Times(0);
  EXPECT_CALL(*logger, set_current_term(_)).Times(0);
  append_entries_response r =
    append_entries(0, 0, 0, std::vector<raft_entry>(), 0, caddr);
  ASSERT_FALSE(r.is_success());
  ASSERT_EQ(r.get_term(), 1);
  ASSERT_EQ(fetch_state(), raft_state::leader);
}

TEST_F(provider_test, GET_LOWER_TERM_2) {
  SetUp(std::set<std::string>{addr});
  usleep(3 * INTERVAL);
  EXPECT_CALL(*logger, set_voted_for_self());
  EXPECT_CALL(*logger, set_current_term(1));
  provider->run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  EXPECT_CALL(*logger, set_voted_for(_)).Times(0);
  EXPECT_CALL(*logger, set_current_term(_)).Times(0);
  request_vote_response r = request_vote(0, caddr, 0, 0);
  ASSERT_FALSE(r.is_vote_granted());
  ASSERT_EQ(r.get_term(), 1);
  ASSERT_EQ(fetch_state(), raft_state::leader);
}

TEST_F(provider_test, NOT_FOUND_PREV_LOG) {
  SetUp(std::set<std::string>{addr});
  ASSERT_EQ(fetch_state(), raft_state::follower);
  EXPECT_CALL(*logger, set_current_term(2));
  append_entries_response r =
    append_entries(2, 1, 1, std::vector<raft_entry>(), 0, caddr);
  ASSERT_EQ(r.get_term(), 2);
  ASSERT_FALSE(r.is_success());
  provider->run();
}

TEST_F(provider_test, CONFLICT_PREV_LOG) {
  SetUp(std::set<std::string>{addr});
  usleep(3 * INTERVAL);
  EXPECT_CALL(*logger, set_voted_for_self());
  EXPECT_CALL(*logger, set_current_term(1));
  provider->run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  EXPECT_CALL(*logger, contains_uuid("046ccc3a-2dac-4e40-ae2e-76797a271fe2"));
  EXPECT_CALL(*logger, append_log("046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                                  "{\"key\":\"foo\",\"value\":\"bar\"}"));
  client_request_response r =
    client_request("046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                   "{\"key\":\"foo\",\"value\":\"bar\"}");
  ASSERT_EQ(r.get_status(), RAFT_SUCCESS);
  ASSERT_EQ(r.get_index(), 2);
  EXPECT_CALL(*logger, set_current_term(2));
  append_entries_response r2 =
    append_entries(2, 2, 2, std::vector<raft_entry>(), 0, caddr);
  ASSERT_FALSE(r2.is_success());
  ASSERT_EQ(r2.get_term(), 2);
  ASSERT_EQ(fetch_state(), raft_state::follower);
}

TEST_F(provider_test, NOT_GRANTED_VOTE_WITH_LATE_LOG) {
  SetUp(std::set<std::string>{addr});
  usleep(3 * INTERVAL);
  EXPECT_CALL(*logger, set_voted_for_self());
  EXPECT_CALL(*logger, set_current_term(1));
  provider->run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  EXPECT_CALL(*logger, contains_uuid("046ccc3a-2dac-4e40-ae2e-76797a271fe2"));
  EXPECT_CALL(*logger, append_log("046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                                  "{\"key\":\"foo\",\"value\":\"bar\"}"));
  client_request_response r =
    client_request("046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                   "{\"key\":\"foo\",\"value\":\"bar\"}");
  ASSERT_EQ(r.get_status(), RAFT_SUCCESS);
  ASSERT_EQ(r.get_index(), 2);
  EXPECT_CALL(*logger, set_current_term(2));
  request_vote_response r2 = request_vote(2, caddr, 1, 0);
  ASSERT_FALSE(r2.is_vote_granted());
  ASSERT_EQ(r2.get_term(), 2);
  ASSERT_EQ(fetch_state(), raft_state::follower);
}

TEST_F(provider_test, NOT_GRANTED_VOTE_WITH_LATE_LOG_2) {
  SetUp(std::set<std::string>{addr});
  usleep(3 * INTERVAL);
  EXPECT_CALL(*logger, set_voted_for_self());
  EXPECT_CALL(*logger, set_current_term(1));
  provider->run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  EXPECT_CALL(*logger, contains_uuid("046ccc3a-2dac-4e40-ae2e-76797a271fe2"));
  EXPECT_CALL(*logger, append_log("046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                                  "{\"key\":\"foo\",\"value\":\"bar\"}"));
  client_request_response r =
    client_request("046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                   "{\"key\":\"foo\",\"value\":\"bar\"}");
  ASSERT_EQ(r.get_status(), RAFT_SUCCESS);
  ASSERT_EQ(r.get_index(), 2);
  EXPECT_CALL(*logger, set_current_term(2));
  request_vote_response r2 = request_vote(2, caddr, 1, 1);
  ASSERT_FALSE(r2.is_vote_granted());
  ASSERT_EQ(r2.get_term(), 2);
  ASSERT_EQ(fetch_state(), raft_state::follower);
}

TEST_F(provider_test, GRANTED_VOTE_WITH_LATEST_LOG) {
  SetUp(std::set<std::string>{addr});
  EXPECT_CALL(*logger, set_voted_for(caddr));
  EXPECT_CALL(*logger, set_current_term(1));
  request_vote_response r2 = request_vote(1, caddr, 1, 1);
  ASSERT_TRUE(r2.is_vote_granted());
  ASSERT_EQ(r2.get_term(), 1);
  ASSERT_EQ(fetch_state(), raft_state::follower);
}

TEST_F(provider_test, GRANTED_VOTE_WITH_LATEST_LOG_2) {
  SetUp(std::set<std::string>{addr});
  usleep(3 * INTERVAL);
  EXPECT_CALL(*logger, set_voted_for_self());
  EXPECT_CALL(*logger, set_current_term(1));
  EXPECT_CALL(*logger, append_log(_, ""));
  provider->run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  EXPECT_CALL(*logger, contains_uuid("046ccc3a-2dac-4e40-ae2e-76797a271fe2"));
  EXPECT_CALL(*logger, append_log("046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                                  "{\"key\":\"foo\",\"value\":\"bar\"}"));
  client_request_response r =
    client_request("046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                   "{\"key\":\"foo\",\"value\":\"bar\"}");
  ASSERT_EQ(r.get_status(), RAFT_SUCCESS);
  ASSERT_EQ(r.get_index(), 2);
  EXPECT_CALL(*logger, set_voted_for(caddr));
  EXPECT_CALL(*logger, set_current_term(2));
  request_vote_response r2 = request_vote(2, caddr, 2, 1);
  ASSERT_TRUE(r2.is_vote_granted());
  ASSERT_EQ(r2.get_term(), 2);
  ASSERT_EQ(fetch_state(), raft_state::follower);
}

TEST_F(provider_test, GRANTED_VOTE_WITH_LATEST_LOG_3) {
  SetUp(std::set<std::string>{addr});
  usleep(3 * INTERVAL);
  EXPECT_CALL(*logger, set_voted_for_self());
  EXPECT_CALL(*logger, set_current_term(1));
  EXPECT_CALL(*logger, append_log(_, ""));
  provider->run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  EXPECT_CALL(*logger, contains_uuid("046ccc3a-2dac-4e40-ae2e-76797a271fe2"));
  EXPECT_CALL(*logger, append_log("046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                                  "{\"key\":\"foo\",\"value\":\"bar\"}"));
  client_request_response r =
    client_request("046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                   "{\"key\":\"foo\",\"value\":\"bar\"}");
  ASSERT_EQ(r.get_status(), RAFT_SUCCESS);
  ASSERT_EQ(r.get_index(), 2);
  EXPECT_CALL(*logger, set_voted_for(caddr));
  EXPECT_CALL(*logger, set_current_term(2));
  request_vote_response r2 = request_vote(2, caddr, 0, 2);
  ASSERT_TRUE(r2.is_vote_granted());
  ASSERT_EQ(r2.get_term(), 2);
  ASSERT_EQ(fetch_state(), raft_state::follower);
}

TEST_F(provider_test, NOT_GRANTED_VOTE_WITH_ALREADY_VOTED) {
  SetUp(std::set<std::string>{addr});
  usleep(3 * INTERVAL);
  EXPECT_CALL(*logger, set_voted_for_self());
  EXPECT_CALL(*logger, set_current_term(1));
  provider->run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  request_vote_response r2 = request_vote(1, caddr, 0, 0);
  ASSERT_FALSE(r2.is_vote_granted());
  ASSERT_EQ(r2.get_term(), 1);
  ASSERT_EQ(fetch_state(), raft_state::leader);
}

TEST_F(provider_test, APPLY_ENTRIES) {
  SetUp(std::set<std::string>{addr});
  EXPECT_CALL(*logger, set_log(1, 1, "046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                               "{\"key\":\"foo\",\"value\":\"bar\"}"));
  std::vector<raft_entry> ent;
  ent.emplace_back(1, 1, "046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                   "{\"key\":\"foo\",\"value\":\"bar\"}");
  append_entries_response r = append_entries(1, 0, 0, ent, 1, caddr);
  ASSERT_EQ(r.get_term(), 1);
  ASSERT_TRUE(r.is_success());
  usleep(3 * INTERVAL);
  EXPECT_CALL(*logger, set_voted_for_self());
  EXPECT_CALL(*logger, set_current_term(2));
  provider->run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  client_query_response r2 = client_query("foo");
  ASSERT_EQ(r2.get_status(), RAFT_SUCCESS);
  ASSERT_STREQ(r2.get_response().c_str(), "bar");
}

TEST_F(provider_test, NOT_DETERMINED_LEADER) {
  SetUp(std::set<std::string>{addr, "127.0.0.1:28888"});
  usleep(3 * INTERVAL);
  EXPECT_CALL(*logger, set_voted_for_self());
  EXPECT_CALL(*logger, set_current_term(1));
  provider->run();
  ASSERT_EQ(fetch_state(), raft_state::candidate);
}

TEST_F(provider_test, BECOME_FOLLOWER_FROM_CANDIDATE) {
  SetUp(std::set<std::string>{addr, "127.0.0.1:28888"});
  usleep(3 * INTERVAL);
  EXPECT_CALL(*logger, set_voted_for_self());
  EXPECT_CALL(*logger, set_current_term(1));
  provider->run();
  ASSERT_EQ(fetch_state(), raft_state::candidate);
  append_entries_response r =
    append_entries(1, 0, 0, std::vector<raft_entry>(), 0, caddr);
  ASSERT_TRUE(r.is_success());
  ASSERT_EQ(r.get_term(), 1);
  ASSERT_EQ(fetch_state(), raft_state::follower);
}

TEST_F(provider_test, CLIENT_GET_LEADER_NOT_FOUND) {
  SetUp(std::set<std::string>{addr, "127.0.0.1:28888"});
  ASSERT_EQ(fetch_state(), raft_state::follower);
  client_query_response r = client_query("hello");
  ASSERT_EQ(r.get_status(), RAFT_LEADER_NOT_FOUND);
  ASSERT_STREQ(r.get_response().c_str(), "");
  usleep(3 * INTERVAL);
  EXPECT_CALL(*logger, set_voted_for_self());
  EXPECT_CALL(*logger, set_current_term(1));
  provider->run();
  ASSERT_EQ(fetch_state(), raft_state::candidate);
  client_query_response r2 = client_query("hello");
  ASSERT_EQ(r2.get_status(), RAFT_LEADER_NOT_FOUND);
  ASSERT_STREQ(r2.get_response().c_str(), "");
}

TEST_F(provider_test, CLIENT_PUT_LEADER_NOT_FOUND) {
  SetUp(std::set<std::string>{addr, "127.0.0.1:28888"});
  ASSERT_EQ(fetch_state(), raft_state::follower);
  client_request_response r =
    client_request("046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                   "{\"key\":\"foo\",\"value\":\"bar\"}");
  ASSERT_EQ(r.get_status(), RAFT_LEADER_NOT_FOUND);
  ASSERT_EQ(r.get_index(), 0);
  usleep(3 * INTERVAL);
  EXPECT_CALL(*logger, set_voted_for_self());
  EXPECT_CALL(*logger, set_current_term(1));
  provider->run();
  ASSERT_EQ(fetch_state(), raft_state::candidate);
  client_request_response r2 =
    client_request("046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                   "{\"key\":\"foo\",\"value\":\"bar\"}");
  ASSERT_EQ(r2.get_status(), RAFT_LEADER_NOT_FOUND);
  ASSERT_EQ(r2.get_index(), 0);
}

TEST_F(provider_test, CANDIDATE_PERMANENTLY) {
  SetUp(std::set<std::string>{addr, "127.0.0.1:28888"});
  ASSERT_EQ(fetch_state(), raft_state::follower);
  usleep(3 * INTERVAL);
  EXPECT_CALL(*logger, set_voted_for_self());
  EXPECT_CALL(*logger, set_current_term(1));
  provider->run();
  ASSERT_EQ(fetch_state(), raft_state::candidate);
  usleep(INTERVAL);
  EXPECT_CALL(*logger, set_voted_for_self());
  EXPECT_CALL(*logger, set_current_term(2));
  provider->run();
  ASSERT_EQ(fetch_state(), raft_state::candidate);
  usleep(INTERVAL);
  provider->run();
  ASSERT_EQ(fetch_state(), raft_state::candidate);
}

TEST_F(provider_test, NODE_IS_NOT_LEADER) {
  SetUp(std::set<std::string>{addr});
  usleep(3 * INTERVAL);
  EXPECT_CALL(*logger, set_voted_for_self());
  EXPECT_CALL(*logger, set_current_term(1));
  provider->run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  EXPECT_CALL(*logger, set_current_term(2));
  append_entries_response r =
    append_entries(2, 0, 0, std::vector<raft_entry>(), 0, caddr);
  ASSERT_TRUE(r.is_success());
  ASSERT_EQ(r.get_term(), 2);
  ASSERT_EQ(fetch_state(), raft_state::follower);
  client_query_response r2 = client_query("hello");
  ASSERT_EQ(r2.get_status(), RAFT_NODE_IS_NOT_LEADER);
  ASSERT_STREQ(r2.get_response().c_str(), "");
  ASSERT_STREQ(r2.get_leader_hint().c_str(), caddr.c_str());
}

TEST_F(provider_test, PUT_INVALID_UUID) {
  SetUp(std::set<std::string>{addr});
  usleep(3 * INTERVAL);
  EXPECT_CALL(*logger, set_voted_for_self());
  EXPECT_CALL(*logger, set_current_term(1));
  provider->run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  client_request_response r =
    client_request("foobarbuz", "{\"key\":\"foo\",\"value\":\"bar\"}");
  ASSERT_EQ(r.get_status(), RAFT_INVALID_UUID);
}

TEST_F(provider_test, TIMEOUT_NOW) {
  SetUp(std::set<std::string>{addr});
  ASSERT_EQ(fetch_state(), raft_state::follower);
  EXPECT_CALL(*logger, set_voted_for_self());
  EXPECT_CALL(*logger, set_current_term(1));
  int err = timeout_now(0, 0, 0);
  ASSERT_EQ(err, RAFT_SUCCESS);
  ASSERT_EQ(fetch_state(), raft_state::leader);
}

TEST_F(provider_test, TIMEOUT_NOW_NOT_FOLLOWER) {
  SetUp(std::set<std::string>{addr});
  usleep(3 * INTERVAL);
  EXPECT_CALL(*logger, set_voted_for_self());
  EXPECT_CALL(*logger, set_current_term(1));
  provider->run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  int err = timeout_now(0, 1, 1);
  ASSERT_EQ(err, RAFT_NODE_IS_NOT_FOLLOWER);
}

TEST_F(provider_test, TIMEOUT_NOW_NOT_FOLLOWER_2) {
  SetUp(std::set<std::string>{addr, "127.0.0.1:28888"});
  usleep(3 * INTERVAL);
  EXPECT_CALL(*logger, set_voted_for_self());
  EXPECT_CALL(*logger, set_current_term(1));
  provider->run();
  ASSERT_EQ(fetch_state(), raft_state::candidate);
  int err = timeout_now(0, 0, 0);
  ASSERT_EQ(err, RAFT_NODE_IS_NOT_FOLLOWER);
  ASSERT_EQ(fetch_state(), raft_state::candidate);
}

TEST_F(provider_test, TIMEOUT_NOW_INVALID_TERM) {
  SetUp(std::set<std::string>{addr});
  ASSERT_EQ(fetch_state(), raft_state::follower);
  int err = timeout_now(1, 0, 0);
  ASSERT_EQ(err, RAFT_INVALID_REQUEST);
  ASSERT_EQ(fetch_state(), raft_state::follower);
}

TEST_F(provider_test, TIMEOUT_NOW_INVALID_TERM_2) {
  SetUp(std::set<std::string>{addr});
  EXPECT_CALL(*logger, set_current_term(1));
  EXPECT_CALL(*logger, set_log(1, 1, "046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                               "{\"key\":\"foo\",\"value\":\"bar\"}"));
  std::vector<raft_entry> ent;
  ent.emplace_back(1, 1, "046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                   "{\"key\":\"foo\",\"value\":\"bar\"}");
  append_entries_response r = append_entries(1, 0, 0, ent, 1, caddr);
  ASSERT_EQ(r.get_term(), 1);
  ASSERT_TRUE(r.is_success());
  ASSERT_EQ(fetch_state(), raft_state::follower);
  int err = timeout_now(0, 1, 1);
  ASSERT_EQ(err, RAFT_INVALID_REQUEST);
  ASSERT_EQ(fetch_state(), raft_state::follower);
}

TEST_F(provider_test, TIMEOUT_NOW_INVALID_PREV) {
  SetUp(std::set<std::string>{addr});
  EXPECT_CALL(*logger, set_log(1, 1, "046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                               "{\"key\":\"foo\",\"value\":\"bar\"}"));
  std::vector<raft_entry> ent;
  ent.emplace_back(1, 1, "046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                   "{\"key\":\"foo\",\"value\":\"bar\"}");
  append_entries_response r = append_entries(1, 0, 0, ent, 1, caddr);
  ASSERT_EQ(r.get_term(), 1);
  ASSERT_TRUE(r.is_success());
  ASSERT_EQ(fetch_state(), raft_state::follower);
  int err = timeout_now(1, 0, 1);
  ASSERT_EQ(err, RAFT_INVALID_REQUEST);
  ASSERT_EQ(fetch_state(), raft_state::follower);
}

TEST_F(provider_test, TIMEOUT_NOW_INVALID_PREV_2) {
  SetUp(std::set<std::string>{addr});
  EXPECT_CALL(*logger, set_log(1, 1, "046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                               "{\"key\":\"foo\",\"value\":\"bar\"}"));
  std::vector<raft_entry> ent;
  ent.emplace_back(1, 1, "046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                   "{\"key\":\"foo\",\"value\":\"bar\"}");
  append_entries_response r = append_entries(1, 0, 0, ent, 1, caddr);
  ASSERT_EQ(r.get_term(), 1);
  ASSERT_TRUE(r.is_success());
  ASSERT_EQ(fetch_state(), raft_state::follower);
  int err = timeout_now(1, 1, 0);
  ASSERT_EQ(err, RAFT_INVALID_REQUEST);
  ASSERT_EQ(fetch_state(), raft_state::follower);
}

TEST_F(provider_test, TIMEOUT_NOW_INVALID_PREV_3) {
  SetUp(std::set<std::string>{addr});
  EXPECT_CALL(*logger, set_log(1, 1, "046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                               "{\"key\":\"foo\",\"value\":\"bar\"}"));
  std::vector<raft_entry> ent;
  ent.emplace_back(1, 1, "046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                   "{\"key\":\"foo\",\"value\":\"bar\"}");
  append_entries_response r = append_entries(1, 0, 0, ent, 1, caddr);
  ASSERT_EQ(r.get_term(), 1);
  ASSERT_TRUE(r.is_success());
  ASSERT_EQ(fetch_state(), raft_state::follower);
  int err = timeout_now(1, 0, 1);
  ASSERT_EQ(err, RAFT_INVALID_REQUEST);
  ASSERT_EQ(fetch_state(), raft_state::follower);
}

TEST_F(provider_test, TIMEOUT_NOW_WITH_HIGHER_LOG) {
  SetUp(std::set<std::string>{addr});
  ASSERT_EQ(fetch_state(), raft_state::follower);
  int err = timeout_now(1, 1, 1);
  ASSERT_EQ(err, RAFT_INVALID_REQUEST);
  ASSERT_EQ(fetch_state(), raft_state::follower);
}

} // namespace