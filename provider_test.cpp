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
  mock_raft_logger(std::string _id)
    : raft_logger(_id, raft_logger_mode::init)
    , real_(_id, raft_logger_mode::init) {
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
    ON_CALL(*this, contains_self_in_nodes())
      .WillByDefault(Invoke(&real_, &lmdb_raft_logger::contains_self_in_nodes));
    ON_CALL(*this, set_current_term(_))
      .WillByDefault(Invoke(&real_, &lmdb_raft_logger::set_current_term));
    ON_CALL(*this, exists_voted_for)
      .WillByDefault(Invoke(&real_, &lmdb_raft_logger::exists_voted_for));
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
    ON_CALL(*this, set_add_conf_log(_))
      .WillByDefault(Invoke(&real_, &lmdb_raft_logger::set_add_conf_log));
    ON_CALL(*this, set_remove_conf_log(_))
      .WillByDefault(Invoke(&real_, &lmdb_raft_logger::set_remove_conf_log));
  }
  ~mock_raft_logger() { real_.clean_up(); }
  MOCK_METHOD0(init, void());
  MOCK_METHOD0(bootstrap, void());
  MOCK_METHOD0(join, void());
  MOCK_METHOD0(clean_up, void());
  MOCK_METHOD0(get_id, std::string());
  MOCK_METHOD0(get_peers, std::set<std::string> &());
  MOCK_METHOD0(get_num_nodes, int());
  MOCK_METHOD0(contains_self_in_nodes, bool());
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
  MOCK_METHOD1(set_add_conf_log, int(const std::string &new_server));
  MOCK_METHOD1(set_remove_conf_log, int(const std::string &old_server));
};

class mock_raft_provider : public tl::provider<mock_raft_provider> {
private:
  void request_vote_rpc(const tl::request &req, int req_term,
                        const std::string &req_candidate_id,
                        int req_last_log_index, int req_last_log_term,
                        bool has_disrupt_permission) {
    req.respond<request_vote_response>({req_term, false});
  }

  void append_entries_rpc(const tl::request &req, int req_term,
                          int req_prev_index, int req_prev_term,
                          const std::vector<raft_entry> &req_entries,
                          int req_leader_commit,
                          const std::string &req_leader_id) {
    req.respond<append_entries_response>({req_term, true});
  }
  void remove_server_rpc(const tl::request &req,
                         const std::string &old_server) {
    req.respond<remove_server_response>({RAFT_SUCCESS, ""});
  }

public:
  tl::remote_procedure m_request_vote_rpc;
  tl::remote_procedure m_append_entries_rpc;
  tl::remote_procedure m_remove_server_rpc;
  mock_raft_provider(tl::engine &e,
                     uint16_t _provider_id = RAFT_PROVIDER_ID + 1)
    : tl::provider<mock_raft_provider>(e, _provider_id)
    , m_request_vote_rpc(
        define("request_vote", &mock_raft_provider::request_vote_rpc))
    , m_append_entries_rpc(
        define("append_entries", &mock_raft_provider::append_entries_rpc))
    , m_remove_server_rpc(
        define("remove_server", &mock_raft_provider::remove_server_rpc)) {}
};

class provider_test : public ::testing::Test {
protected:
  std::random_device rnd;
  int PORT;
  std::string addr, caddr;
  tl::engine server_engine;
  tl::engine client_engine;
  mock_raft_logger logger;
  mock_raft_fsm fsm;
  raft_provider provider;
  mock_raft_provider m_provider;
  tl::remote_procedure m_echo_state_rpc;
  tl::remote_procedure m_request_vote_rpc;
  tl::remote_procedure m_append_entries_rpc;
  tl::remote_procedure m_timeout_now_rpc;
  tl::remote_procedure m_client_request_rpc;
  tl::remote_procedure m_client_query_rpc;
  tl::remote_procedure m_add_server_rpc;
  tl::remote_procedure m_remove_server_rpc;
  tl::provider_handle server_addr;
  provider_test()
    : PORT(rnd() % 1000 + 30000)
    , addr(ADDR + std::to_string(PORT))
    , caddr(ADDR + std::to_string(PORT + 1))
    , server_engine(PROTOCOL_PREFIX + addr, THALLIUM_SERVER_MODE, true, 2)
    , client_engine(PROTOCOL_PREFIX + caddr, THALLIUM_SERVER_MODE, true)
    , logger(addr)
    , fsm()
    , provider(server_engine, &logger, &fsm)
    , m_provider(client_engine)
    , m_echo_state_rpc(client_engine.define(ECHO_STATE_RPC_NAME))
    , m_request_vote_rpc(client_engine.define("request_vote"))
    , m_append_entries_rpc(client_engine.define("append_entries"))
    , m_timeout_now_rpc(client_engine.define("timeout_now"))
    , m_client_request_rpc(client_engine.define(CLIENT_REQUEST_RPC_NAME))
    , m_client_query_rpc(client_engine.define(CLIENT_QUERY_RPC_NAME))
    , m_add_server_rpc(client_engine.define("add_server"))
    , m_remove_server_rpc(client_engine.define("remove_server"))
    , server_addr(tl::provider_handle(
        client_engine.lookup(PROTOCOL_PREFIX + addr), RAFT_PROVIDER_ID)) {
    std::cout << "server running at " << server_engine.self() << std::endl;
  }

  void TearDown() {
    server_addr = tl::provider_handle();
    m_echo_state_rpc.deregister();
    m_request_vote_rpc.deregister();
    m_append_entries_rpc.deregister();
    m_timeout_now_rpc.deregister();
    m_add_server_rpc.deregister();
    m_remove_server_rpc.deregister();
    m_client_request_rpc.deregister();
    m_client_query_rpc.deregister();
    bool ok = provider.remove_self_from_cluster();
    ASSERT_TRUE(ok);
    printf("client engine finalize");
    client_engine.finalize();
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
                                     int last_log_index, int last_log_term,
                                     bool has_disrupt_permission = false) {
    request_vote_response resp =
      m_request_vote_rpc.on(server_addr)(term, candidate_id, last_log_index,
                                         last_log_term, has_disrupt_permission);
    return resp;
  }

  int timeout_now(int term, int prev_index, int prev_term) {
    int err = m_timeout_now_rpc.on(server_addr)(term, prev_index, prev_term);
    return err;
  }

  client_request_response client_request(std::string uuid,
                                         std::string command) {
    printf("client_request start\n");
    tl::async_response req =
      m_client_request_rpc.on(server_addr).async(uuid, command);
    printf("sleep\n");
    usleep(INTERVAL);
    printf("run 1\n");
    provider.run();
    printf("run 2\n");
    usleep(INTERVAL);
    provider.run();
    printf("wait response\n ");
    client_request_response resp = req.wait();
    return resp;
  }

  void add_dummy_server() {
    printf("add_dummy_server start");
    usleep(3 * INTERVAL);
    provider.run();
    ASSERT_EQ(fetch_state(), raft_state::leader);
    provider.run();
    provider.run();

    tl::async_response req = m_add_server_rpc.on(server_addr).async(caddr);
    printf("sleep\n");
    usleep(3 * INTERVAL);
    printf("run 1\n");
    provider.run();
    printf("run 2\n");
    usleep(INTERVAL);
    provider.run();
    printf("wait response\n ");
    add_server_response resp = req.wait();
    ASSERT_EQ(resp.status, RAFT_SUCCESS);
    // become follower
    append_entries_response r2 =
      append_entries(2, 2, 1, std::vector<raft_entry>(), 0, caddr);
    ASSERT_TRUE(r2.success);
    ASSERT_EQ(r2.term, 2);
    ASSERT_EQ(fetch_state(), raft_state::follower);
  }

  client_query_response client_query(std::string query) {
    client_query_response resp = m_client_query_rpc.on(server_addr)(query);
    return resp;
  }

  add_server_response add_server(std::string new_server) {
    printf("add server start\n");
    tl::async_response req = m_add_server_rpc.on(server_addr).async(new_server);
    printf("sleep\n");
    usleep(INTERVAL);
    printf("run 1\n");
    provider.run();
    printf("run 2\n");
    usleep(INTERVAL);
    add_server_response resp = req.wait();
    return resp;
  }

  remove_server_response remove_server(std::string old_server) {
    printf("remove server start\n");
    tl::async_response req =
      m_remove_server_rpc.on(server_addr).async(old_server);
    printf("sleep\n");
    usleep(INTERVAL);
    printf("run 1\n");
    provider.run();
    printf("run 2\n");
    usleep(INTERVAL);
    remove_server_response resp = req.wait();
    return resp;
  }
};

TEST_F(provider_test, BECOME_FOLLOWER) {
  ASSERT_EQ(fetch_state(), raft_state::follower);
}

TEST_F(provider_test, BECOME_LEADER) {
  usleep(3 * INTERVAL);
  EXPECT_CALL(logger, set_voted_for_self());
  EXPECT_CALL(logger, set_current_term(1));
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
}

TEST_F(provider_test, QUERY_RPC) {
  usleep(3 * INTERVAL);
  EXPECT_CALL(logger, set_voted_for_self());
  EXPECT_CALL(logger, set_current_term(1));
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  client_query_response r = client_query("hello");
  ASSERT_EQ(r.status, RAFT_SUCCESS);
  ASSERT_STREQ(r.response.c_str(), "world");
}

TEST_F(provider_test, REQUEST_RPC) {
  usleep(3 * INTERVAL);
  EXPECT_CALL(logger, set_voted_for_self());
  EXPECT_CALL(logger, set_current_term(1));
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  EXPECT_CALL(logger, contains_uuid("046ccc3a-2dac-4e40-ae2e-76797a271fe2"));
  EXPECT_CALL(logger, append_log("046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                                 "{\"key\":\"foo\",\"value\":\"bar\"}"));
  client_request_response r =
    client_request("046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                   "{\"key\":\"foo\",\"value\":\"bar\"}");
  ASSERT_EQ(r.status, RAFT_SUCCESS);

  provider.run(); // applied "foo" "bar"
  client_query_response r2 = client_query("foo");
  ASSERT_EQ(r2.status, RAFT_SUCCESS);
  ASSERT_STREQ(r2.response.c_str(), "bar");
}

TEST_F(provider_test, REQUEST_RPC_LEADER_NOT_FOUND) {
  ASSERT_EQ(fetch_state(), raft_state::follower);
  client_request_response r =
    client_request("046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                   "{\"key\":\"foo\",\"value\":\"bar\"}");
  ASSERT_EQ(r.status, RAFT_LEADER_NOT_FOUND);
}

TEST_F(provider_test, GET_HIGHER_TERM) {
  usleep(3 * INTERVAL);
  EXPECT_CALL(logger, set_voted_for_self());
  EXPECT_CALL(logger, set_current_term(1));
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  EXPECT_CALL(logger, set_current_term(2));
  append_entries_response r =
    append_entries(2, 0, 0, std::vector<raft_entry>(), 0, caddr);
  ASSERT_TRUE(r.success);
  ASSERT_EQ(r.term, 2);
  ASSERT_EQ(fetch_state(), raft_state::follower);
}

TEST_F(provider_test, GET_HIGHER_TERM_2) {
  usleep(3 * INTERVAL);
  EXPECT_CALL(logger, set_voted_for_self());
  EXPECT_CALL(logger, set_current_term(1));
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  EXPECT_CALL(logger, set_voted_for(caddr));
  EXPECT_CALL(logger, set_current_term(2));
  request_vote_response r = request_vote(2, caddr, 2, 2, true);
  ASSERT_TRUE(r.vote_granted);
  ASSERT_EQ(r.term, 2);
  ASSERT_EQ(fetch_state(), raft_state::follower);
}

TEST_F(provider_test, GET_LOWER_TERM) {
  usleep(3 * INTERVAL);
  EXPECT_CALL(logger, set_voted_for_self());
  EXPECT_CALL(logger, set_current_term(1));
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  EXPECT_CALL(logger, set_voted_for(_)).Times(0);
  EXPECT_CALL(logger, set_current_term(_)).Times(0);
  append_entries_response r =
    append_entries(0, 0, 0, std::vector<raft_entry>(), 0, caddr);
  ASSERT_FALSE(r.success);
  ASSERT_EQ(r.term, 1);
  ASSERT_EQ(fetch_state(), raft_state::leader);
}

TEST_F(provider_test, GET_LOWER_TERM_2) {
  usleep(3 * INTERVAL);
  EXPECT_CALL(logger, set_voted_for_self());
  EXPECT_CALL(logger, set_current_term(1));
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  EXPECT_CALL(logger, set_voted_for(_)).Times(0);
  EXPECT_CALL(logger, set_current_term(_)).Times(0);
  request_vote_response r = request_vote(0, caddr, 0, 0);
  ASSERT_FALSE(r.vote_granted);
  ASSERT_EQ(r.term, 1);
  ASSERT_EQ(fetch_state(), raft_state::leader);
}

TEST_F(provider_test, NOT_FOUND_PREV_LOG) {
  ASSERT_EQ(fetch_state(), raft_state::follower);
  EXPECT_CALL(logger, set_current_term(2));
  append_entries_response r =
    append_entries(2, 1, 1, std::vector<raft_entry>(), 0, caddr);
  ASSERT_EQ(r.term, 2);
  ASSERT_FALSE(r.success);
  provider.run();
}

TEST_F(provider_test, CONFLICT_PREV_LOG) {
  usleep(3 * INTERVAL);
  EXPECT_CALL(logger, set_voted_for_self());
  EXPECT_CALL(logger, set_current_term(1));
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  EXPECT_CALL(logger, contains_uuid("046ccc3a-2dac-4e40-ae2e-76797a271fe2"));
  EXPECT_CALL(logger, append_log("046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                                 "{\"key\":\"foo\",\"value\":\"bar\"}"));
  client_request_response r =
    client_request("046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                   "{\"key\":\"foo\",\"value\":\"bar\"}");
  ASSERT_EQ(r.status, RAFT_SUCCESS);

  EXPECT_CALL(logger, set_current_term(2));
  append_entries_response r2 =
    append_entries(2, 2, 2, std::vector<raft_entry>(), 0, caddr);
  ASSERT_FALSE(r2.success);
  ASSERT_EQ(r2.term, 2);
  ASSERT_EQ(fetch_state(), raft_state::follower);
}

TEST_F(provider_test, NOT_GRANTED_VOTE_WITH_LATE_LOG) {
  usleep(3 * INTERVAL);
  EXPECT_CALL(logger, set_voted_for_self());
  EXPECT_CALL(logger, set_current_term(1));
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  EXPECT_CALL(logger, contains_uuid("046ccc3a-2dac-4e40-ae2e-76797a271fe2"));
  EXPECT_CALL(logger, append_log("046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                                 "{\"key\":\"foo\",\"value\":\"bar\"}"));
  client_request_response r =
    client_request("046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                   "{\"key\":\"foo\",\"value\":\"bar\"}");
  ASSERT_EQ(r.status, RAFT_SUCCESS);

  EXPECT_CALL(logger, set_current_term(2));
  request_vote_response r2 = request_vote(2, caddr, 1, 0);
  ASSERT_FALSE(r2.vote_granted);
  ASSERT_EQ(r2.term, 2);
  ASSERT_EQ(fetch_state(), raft_state::follower);
}

TEST_F(provider_test, NOT_GRANTED_VOTE_WITH_LATE_LOG_2) {
  usleep(3 * INTERVAL);
  EXPECT_CALL(logger, set_voted_for_self());
  EXPECT_CALL(logger, set_current_term(1));
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  EXPECT_CALL(logger, contains_uuid("046ccc3a-2dac-4e40-ae2e-76797a271fe2"));
  EXPECT_CALL(logger, append_log("046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                                 "{\"key\":\"foo\",\"value\":\"bar\"}"));
  client_request_response r =
    client_request("046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                   "{\"key\":\"foo\",\"value\":\"bar\"}");
  ASSERT_EQ(r.status, RAFT_SUCCESS);

  EXPECT_CALL(logger, set_current_term(2));
  request_vote_response r2 = request_vote(2, caddr, 1, 1);
  ASSERT_FALSE(r2.vote_granted);
  ASSERT_EQ(r2.term, 2);
  ASSERT_EQ(fetch_state(), raft_state::follower);
}

TEST_F(provider_test, GRANTED_VOTE_WITH_LATEST_LOG) {
  EXPECT_CALL(logger, set_voted_for(caddr));
  EXPECT_CALL(logger, set_current_term(1));
  request_vote_response r2 = request_vote(1, caddr, 1, 1);
  ASSERT_TRUE(r2.vote_granted);
  ASSERT_EQ(r2.term, 1);
  ASSERT_EQ(fetch_state(), raft_state::follower);
}

TEST_F(provider_test, GRANTED_VOTE_WITH_LATEST_LOG_2) {
  usleep(3 * INTERVAL);
  EXPECT_CALL(logger, set_voted_for_self());
  EXPECT_CALL(logger, set_current_term(1));
  EXPECT_CALL(logger, append_log(_, ""));
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  EXPECT_CALL(logger, contains_uuid("046ccc3a-2dac-4e40-ae2e-76797a271fe2"));
  EXPECT_CALL(logger, append_log("046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                                 "{\"key\":\"foo\",\"value\":\"bar\"}"));
  EXPECT_CALL(logger, set_current_term(::testing::Ge(2)))
    .Times(::testing::AnyNumber());
  client_request_response r =
    client_request("046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                   "{\"key\":\"foo\",\"value\":\"bar\"}");
  ASSERT_EQ(r.status, RAFT_SUCCESS);

  EXPECT_CALL(logger, set_voted_for(caddr));
  request_vote_response r2 = request_vote(2, caddr, 3, 1, true);
  ASSERT_TRUE(r2.vote_granted);
  ASSERT_EQ(r2.term, 2);
  ASSERT_EQ(fetch_state(), raft_state::follower);
}

TEST_F(provider_test, GRANTED_VOTE_WITH_LATEST_LOG_3) {
  usleep(3 * INTERVAL);
  EXPECT_CALL(logger, set_voted_for_self());
  EXPECT_CALL(logger, set_current_term(1));
  EXPECT_CALL(logger, append_log(_, ""));
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  EXPECT_CALL(logger, contains_uuid("046ccc3a-2dac-4e40-ae2e-76797a271fe2"));
  EXPECT_CALL(logger, append_log("046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                                 "{\"key\":\"foo\",\"value\":\"bar\"}"));
  client_request_response r =
    client_request("046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                   "{\"key\":\"foo\",\"value\":\"bar\"}");
  ASSERT_EQ(r.status, RAFT_SUCCESS);

  EXPECT_CALL(logger, set_voted_for(caddr));
  EXPECT_CALL(logger, set_current_term(2));
  request_vote_response r2 = request_vote(2, caddr, 0, 2);
  ASSERT_TRUE(r2.vote_granted);
  ASSERT_EQ(r2.term, 2);
  ASSERT_EQ(fetch_state(), raft_state::follower);
}

TEST_F(provider_test, NOT_GRANTED_VOTE_WITH_ALREADY_VOTED) {
  usleep(3 * INTERVAL);
  EXPECT_CALL(logger, set_voted_for_self());
  EXPECT_CALL(logger, set_current_term(1));
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  request_vote_response r2 = request_vote(1, caddr, 0, 0);
  ASSERT_FALSE(r2.vote_granted);
  ASSERT_EQ(r2.term, 1);
  ASSERT_EQ(fetch_state(), raft_state::leader);
}

TEST_F(provider_test, APPLY_ENTRIES) {
  EXPECT_CALL(logger, set_log(2, 1, "046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                              "{\"key\":\"foo\",\"value\":\"bar\"}"));
  std::vector<raft_entry> ent;
  ent.emplace_back(2, 1, "046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                   "{\"key\":\"foo\",\"value\":\"bar\"}");
  append_entries_response r = append_entries(1, 1, 0, ent, 1, caddr);
  ASSERT_EQ(r.term, 1);
  ASSERT_TRUE(r.success);
  usleep(3 * INTERVAL);
  EXPECT_CALL(logger, set_voted_for_self());
  EXPECT_CALL(logger, set_current_term(2));
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  client_query_response r2 = client_query("foo");
  ASSERT_EQ(r2.status, RAFT_SUCCESS);
  ASSERT_STREQ(r2.response.c_str(), "bar");
}

TEST_F(provider_test, NOT_DETERMINED_LEADER) {
  add_dummy_server();
  usleep(3 * INTERVAL);
  provider.run();
  ASSERT_NE(fetch_state(), raft_state::leader);
}

TEST_F(provider_test, BECOME_FOLLOWER_FROM_CANDIDATE) {
  add_dummy_server();
  provider.run();
  ASSERT_NE(fetch_state(), raft_state::leader);
  append_entries_response r =
    append_entries(3, 100, 3, std::vector<raft_entry>(), 0, caddr);
  ASSERT_FALSE(r.success);
  ASSERT_EQ(r.term, 3);
  ASSERT_EQ(fetch_state(), raft_state::follower);
}

TEST_F(provider_test, CLIENT_GET_LEADER_NOT_FOUND) {
  ASSERT_EQ(fetch_state(), raft_state::follower);
  client_query_response r = client_query("hello");
  ASSERT_EQ(r.status, RAFT_LEADER_NOT_FOUND);
}

TEST_F(provider_test, CLIENT_PUT_LEADER_NOT_FOUND) {
  ASSERT_EQ(fetch_state(), raft_state::follower);
  client_request_response r =
    client_request("046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                   "{\"key\":\"foo\",\"value\":\"bar\"}");
  ASSERT_EQ(r.status, RAFT_LEADER_NOT_FOUND);
}

TEST_F(provider_test, CLIENT_PUT_NODE_IS_NOT_LEADER) {
  append_entries_response r =
    append_entries(1, 0, 0, std::vector<raft_entry>(), 0, caddr);
  ASSERT_TRUE(r.success);
  ASSERT_EQ(r.term, 1);
  ASSERT_EQ(fetch_state(), raft_state::follower);
  client_request_response r2 =
    client_request("046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                   "{\"key\":\"foo\",\"value\":\"bar\"}");
  ASSERT_EQ(r2.status, RAFT_NODE_IS_NOT_LEADER);

  ASSERT_STREQ(r2.leader_hint.c_str(), caddr.c_str());
}

TEST_F(provider_test, CANDIDATE_PERMANENTLY) {
  add_dummy_server();
  ASSERT_EQ(fetch_state(), raft_state::follower);
  usleep(3 * INTERVAL);
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::candidate);
  usleep(INTERVAL);
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::candidate);
  usleep(INTERVAL);
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::candidate);
}

TEST_F(provider_test, NODE_IS_NOT_LEADER) {
  usleep(3 * INTERVAL);
  EXPECT_CALL(logger, set_voted_for_self());
  EXPECT_CALL(logger, set_current_term(1));
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  EXPECT_CALL(logger, set_current_term(2));
  append_entries_response r =
    append_entries(2, 0, 0, std::vector<raft_entry>(), 0, caddr);
  ASSERT_TRUE(r.success);
  ASSERT_EQ(r.term, 2);
  ASSERT_EQ(fetch_state(), raft_state::follower);
  client_query_response r2 = client_query("hello");
  ASSERT_EQ(r2.status, RAFT_NODE_IS_NOT_LEADER);
  ASSERT_STREQ(r2.response.c_str(), "");
  ASSERT_STREQ(r2.leader_hint.c_str(), caddr.c_str());
}

TEST_F(provider_test, PUT_INVALID_UUID) {
  usleep(3 * INTERVAL);
  EXPECT_CALL(logger, set_voted_for_self());
  EXPECT_CALL(logger, set_current_term(1));
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  client_request_response r =
    client_request("foobarbuz", "{\"key\":\"foo\",\"value\":\"bar\"}");
  ASSERT_EQ(r.status, RAFT_INVALID_UUID);
}

TEST_F(provider_test, TIMEOUT_NOW) {
  ASSERT_EQ(fetch_state(), raft_state::follower);
  EXPECT_CALL(logger, set_voted_for_self());
  EXPECT_CALL(logger, set_current_term(1));
  int err = timeout_now(0, 0, 0);
  ASSERT_EQ(err, RAFT_SUCCESS);
  ASSERT_EQ(fetch_state(), raft_state::leader);
}

TEST_F(provider_test, TIMEOUT_NOW_NOT_FOLLOWER) {
  usleep(3 * INTERVAL);
  EXPECT_CALL(logger, set_voted_for_self());
  EXPECT_CALL(logger, set_current_term(1));
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  int err = timeout_now(1, 1, 1);
  ASSERT_EQ(err, RAFT_NODE_IS_NOT_FOLLOWER);
}

TEST_F(provider_test, TIMEOUT_NOW_NOT_FOLLOWER_2) {
  add_dummy_server();
  usleep(3 * INTERVAL);
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::candidate);
  int err = timeout_now(3, 0, 0);
  ASSERT_EQ(err, RAFT_NODE_IS_NOT_FOLLOWER);
  ASSERT_EQ(fetch_state(), raft_state::candidate);
}

TEST_F(provider_test, TIMEOUT_NOW_INVALID_TERM) {
  ASSERT_EQ(fetch_state(), raft_state::follower);
  int err = timeout_now(1, 0, 0);
  ASSERT_EQ(err, RAFT_INVALID_REQUEST);
  ASSERT_EQ(fetch_state(), raft_state::follower);
}

TEST_F(provider_test, TIMEOUT_NOW_INVALID_TERM_2) {
  EXPECT_CALL(logger, set_current_term(1));
  EXPECT_CALL(logger, set_log(1, 1, "046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                              "{\"key\":\"foo\",\"value\":\"bar\"}"));
  std::vector<raft_entry> ent;
  ent.emplace_back(1, 1, "046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                   "{\"key\":\"foo\",\"value\":\"bar\"}");
  append_entries_response r = append_entries(1, 0, 0, ent, 1, caddr);
  ASSERT_EQ(r.term, 1);
  ASSERT_TRUE(r.success);
  ASSERT_EQ(fetch_state(), raft_state::follower);
  int err = timeout_now(0, 1, 1);
  ASSERT_EQ(err, RAFT_INVALID_REQUEST);
  ASSERT_EQ(fetch_state(), raft_state::follower);
}

TEST_F(provider_test, TIMEOUT_NOW_INVALID_PREV) {
  EXPECT_CALL(logger, set_log(1, 1, "046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                              "{\"key\":\"foo\",\"value\":\"bar\"}"));
  std::vector<raft_entry> ent;
  ent.emplace_back(1, 1, "046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                   "{\"key\":\"foo\",\"value\":\"bar\"}");
  append_entries_response r = append_entries(1, 0, 0, ent, 1, caddr);
  ASSERT_EQ(r.term, 1);
  ASSERT_TRUE(r.success);
  ASSERT_EQ(fetch_state(), raft_state::follower);
  int err = timeout_now(1, 0, 1);
  ASSERT_EQ(err, RAFT_INVALID_REQUEST);
  ASSERT_EQ(fetch_state(), raft_state::follower);
}

TEST_F(provider_test, TIMEOUT_NOW_INVALID_PREV_2) {
  EXPECT_CALL(logger, set_log(1, 1, "046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                              "{\"key\":\"foo\",\"value\":\"bar\"}"));
  std::vector<raft_entry> ent;
  ent.emplace_back(1, 1, "046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                   "{\"key\":\"foo\",\"value\":\"bar\"}");
  append_entries_response r = append_entries(1, 0, 0, ent, 1, caddr);
  ASSERT_EQ(r.term, 1);
  ASSERT_TRUE(r.success);
  ASSERT_EQ(fetch_state(), raft_state::follower);
  int err = timeout_now(1, 1, 0);
  ASSERT_EQ(err, RAFT_INVALID_REQUEST);
  ASSERT_EQ(fetch_state(), raft_state::follower);
}

TEST_F(provider_test, TIMEOUT_NOW_INVALID_PREV_3) {
  EXPECT_CALL(logger, set_log(1, 1, "046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                              "{\"key\":\"foo\",\"value\":\"bar\"}"));
  std::vector<raft_entry> ent;
  ent.emplace_back(1, 1, "046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                   "{\"key\":\"foo\",\"value\":\"bar\"}");
  append_entries_response r = append_entries(1, 0, 0, ent, 1, caddr);
  ASSERT_EQ(r.term, 1);
  ASSERT_TRUE(r.success);
  ASSERT_EQ(fetch_state(), raft_state::follower);
  int err = timeout_now(1, 0, 1);
  ASSERT_EQ(err, RAFT_INVALID_REQUEST);
  ASSERT_EQ(fetch_state(), raft_state::follower);
}

TEST_F(provider_test, TIMEOUT_NOW_WITH_HIGHER_LOG) {
  ASSERT_EQ(fetch_state(), raft_state::follower);
  int err = timeout_now(1, 1, 1);
  ASSERT_EQ(err, RAFT_INVALID_REQUEST);
  ASSERT_EQ(fetch_state(), raft_state::follower);
}

} // namespace