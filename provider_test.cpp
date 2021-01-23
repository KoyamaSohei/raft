#include "provider.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <random>
#include <thallium.hpp>

#include "logger.hpp"
#include "types.hpp"

#define ADDR "127.0.0.1:"

namespace {

class mock_raft_logger : public raft_logger {
private:
  lmdb_raft_logger real_;

public:
  mock_raft_logger(std::string _id) : raft_logger(_id), real_(_id) {
    ON_CALL(*this, init(::testing::_))
      .WillByDefault(::testing::Invoke(&real_, &lmdb_raft_logger::init));
    ON_CALL(*this,
            bootstrap_state_from_log(::testing::_, ::testing::_, ::testing::_))
      .WillByDefault(
        ::testing::Invoke(&real_, &lmdb_raft_logger::bootstrap_state_from_log));
    ON_CALL(*this, save_current_term(::testing::_))
      .WillByDefault(
        ::testing::Invoke(&real_, &lmdb_raft_logger::save_current_term));
    ON_CALL(*this, save_voted_for(::testing::_))
      .WillByDefault(
        ::testing::Invoke(&real_, &lmdb_raft_logger::save_voted_for));
    ON_CALL(*this, get_log(::testing::_, ::testing::_, ::testing::_,
                           ::testing::_, ::testing::_))
      .WillByDefault(::testing::Invoke(&real_, &lmdb_raft_logger::get_log));
    ON_CALL(*this, save_log(::testing::_, ::testing::_, ::testing::_,
                            ::testing::_, ::testing::_))
      .WillByDefault(::testing::Invoke(&real_, &lmdb_raft_logger::save_log));
    ON_CALL(*this,
            append_log(::testing::_, ::testing::_, ::testing::_, ::testing::_))
      .WillByDefault(::testing::Invoke(&real_, &lmdb_raft_logger::append_log));
    ON_CALL(*this, get_term(::testing::_))
      .WillByDefault(::testing::Invoke(&real_, &lmdb_raft_logger::get_term));
    ON_CALL(*this, get_last_log(::testing::_, ::testing::_))
      .WillByDefault(
        ::testing::Invoke(&real_, &lmdb_raft_logger::get_last_log));
    ON_CALL(*this, match_log(::testing::_, ::testing::_))
      .WillByDefault(::testing::Invoke(&real_, &lmdb_raft_logger::match_log));
    ON_CALL(*this, uuid_already_exists(::testing::_))
      .WillByDefault(
        ::testing::Invoke(&real_, &lmdb_raft_logger::uuid_already_exists));
    ON_CALL(*this, generate_uuid())
      .WillByDefault(
        ::testing::Invoke(&real_, &lmdb_raft_logger::generate_uuid));
  }
  ~mock_raft_logger() {
    int err;
    std::string dir_path = real_.generate_path(id);
    std::string data_path = dir_path + "/data.mdb";
    std::string lock_path = dir_path + "/lock.mdb";

    err = remove(data_path.c_str());
    if (err) { abort(); }

    err = remove(lock_path.c_str());
    if (err) { abort(); }

    err = rmdir(dir_path.c_str());
    if (err) { abort(); }
  }
  MOCK_METHOD1(init, void(std::string addrs));
  MOCK_METHOD2(get_nodes_from_buf,
               void(std::string buf, std::vector<std::string> &nodes));
  MOCK_METHOD2(get_buf_from_nodes,
               void(std::string &buf, std::vector<std::string> nodes));
  MOCK_METHOD3(bootstrap_state_from_log,
               void(int &current_term, std::string &voted_for,
                    std::vector<std::string> &nodes));
  MOCK_METHOD1(save_current_term, void(int current_term));
  MOCK_METHOD1(save_voted_for, void(std::string voted_for));
  MOCK_METHOD5(get_log, void(int index, int &term, std::string &uuid,
                             std::string &key, std::string &value));
  MOCK_METHOD5(save_log, void(int index, int term, std::string uuid,
                              std::string key, std::string value));
  MOCK_METHOD4(append_log, int(int term, std::string uuid, std::string key,
                               std::string value));
  MOCK_METHOD1(get_term, int(int index));
  MOCK_METHOD2(get_last_log, void(int &index, int &term));
  MOCK_METHOD2(match_log, bool(int index, int term));
  MOCK_METHOD1(uuid_already_exists, bool(std::string uuid));
  MOCK_METHOD0(generate_uuid, std::string());
};

class provider_test : public ::testing::Test {
protected:
  std::random_device rnd;
  int PORT;
  std::string addr, caddr;
  tl::abt scope;
  tl::engine server_engine;
  tl::engine client_engine;
  mock_raft_logger logger;
  raft_provider provider;
  tl::remote_procedure m_echo_state_rpc;
  tl::remote_procedure m_request_vote_rpc;
  tl::remote_procedure m_append_entries_rpc;
  tl::remote_procedure m_client_put_rpc;
  tl::remote_procedure m_client_get_rpc;
  tl::provider_handle server_addr;
  provider_test()
    : PORT(rnd() % 1000 + 30000)
    , addr("ofi+sockets://" ADDR + std::to_string(PORT))
    , caddr("ofi+sockets://" ADDR + std::to_string(PORT + 1))
    , server_engine(addr, THALLIUM_SERVER_MODE, true, 2)
    , client_engine(caddr, THALLIUM_CLIENT_MODE)
    , logger(server_engine.self())
    , provider(server_engine, &logger, RAFT_PROVIDER_ID)
    , m_echo_state_rpc(client_engine.define(ECHO_STATE_RPC_NAME))
    , m_request_vote_rpc(client_engine.define("request_vote"))
    , m_append_entries_rpc(client_engine.define("append_entries"))
    , m_client_put_rpc(client_engine.define(CLIENT_PUT_RPC_NAME))
    , m_client_get_rpc(client_engine.define(CLIENT_GET_RPC_NAME))
    , server_addr(
        tl::provider_handle(client_engine.lookup(addr), RAFT_PROVIDER_ID)) {
    std::cout << "server running at " << server_engine.self() << std::endl;
  }

  void SetUp() {
    EXPECT_CALL(logger, save_voted_for("")).Times(::testing::AnyNumber());
  }

  static void finalize(void *arg) { ((raft_provider *)arg)->finalize(); }

  ~provider_test() {}
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

  client_put_response client_put(std::string uuid, std::string key,
                                 std::string value) {
    tl::async_response req =
      m_client_put_rpc.on(server_addr).async(uuid, key, value);

    usleep(INTERVAL);
    // to commit log in run_leader
    provider.run();
    provider.run();

    client_put_response resp = req.wait();
    return resp;
  }

  client_get_response client_get(std::string key) {
    client_get_response resp = m_client_get_rpc.on(server_addr)(key);
    return resp;
  }
};

TEST_F(provider_test, BECOME_FOLLOWER) {
  logger.init(addr);
  provider.start();
  ASSERT_EQ(fetch_state(), raft_state::follower);
}

TEST_F(provider_test, BECOME_LEADER) {
  logger.init(addr);
  provider.start();
  usleep(3 * INTERVAL);
  EXPECT_CALL(logger, save_voted_for(addr));
  EXPECT_CALL(logger, save_current_term(1));
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
}

TEST_F(provider_test, GET_RPC) {
  logger.init(addr);
  provider.start();
  usleep(3 * INTERVAL);
  EXPECT_CALL(logger, save_voted_for(addr));
  EXPECT_CALL(logger, save_current_term(1));
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  client_get_response r = client_get("hello");
  ASSERT_EQ(r.get_error(), RAFT_SUCCESS);
  ASSERT_STREQ(r.get_value().c_str(), "world");
}

TEST_F(provider_test, PUT_RPC) {
  logger.init(addr);
  provider.start();
  usleep(3 * INTERVAL);
  EXPECT_CALL(logger, save_voted_for(addr));
  EXPECT_CALL(logger, save_current_term(1));
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  EXPECT_CALL(logger,
              uuid_already_exists("046ccc3a-2dac-4e40-ae2e-76797a271fe2"));
  EXPECT_CALL(logger, append_log(1, "046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                                 "foo", "bar"));
  client_put_response r =
    client_put("046ccc3a-2dac-4e40-ae2e-76797a271fe2", "foo", "bar");
  ASSERT_EQ(r.get_error(), RAFT_SUCCESS);
  ASSERT_EQ(r.get_index(), 2);
  provider.run(); // applied "foo" "bar"
  client_get_response r2 = client_get("foo");
  ASSERT_EQ(r2.get_error(), RAFT_SUCCESS);
  ASSERT_STREQ(r2.get_value().c_str(), "bar");
}

TEST_F(provider_test, PUT_RPC_LEADER_NOT_FOUND) {
  logger.init(addr);
  provider.start();
  ASSERT_EQ(fetch_state(), raft_state::follower);
  client_put_response r =
    client_put("046ccc3a-2dac-4e40-ae2e-76797a271fe2", "foo", "bar");
  ASSERT_EQ(r.get_error(), RAFT_LEADER_NOT_FOUND);
  ASSERT_EQ(r.get_index(), 0);
}

TEST_F(provider_test, GET_HIGHER_TERM) {
  logger.init(addr);
  provider.start();
  usleep(3 * INTERVAL);
  EXPECT_CALL(logger, save_voted_for(addr));
  EXPECT_CALL(logger, save_current_term(1));
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  EXPECT_CALL(logger, save_current_term(2));
  append_entries_response r =
    append_entries(2, 0, 0, std::vector<raft_entry>(), 0, caddr);
  ASSERT_TRUE(r.is_success());
  ASSERT_EQ(r.get_term(), 2);
  ASSERT_EQ(fetch_state(), raft_state::follower);
}

TEST_F(provider_test, GET_HIGHER_TERM_2) {
  logger.init(addr);
  provider.start();
  usleep(3 * INTERVAL);
  EXPECT_CALL(logger, save_voted_for(addr));
  EXPECT_CALL(logger, save_current_term(1));
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  EXPECT_CALL(logger, save_voted_for(caddr));
  EXPECT_CALL(logger, save_current_term(2));
  request_vote_response r = request_vote(2, caddr, 1, 1);
  ASSERT_TRUE(r.is_vote_granted());
  ASSERT_EQ(r.get_term(), 2);
  ASSERT_EQ(fetch_state(), raft_state::follower);
}

TEST_F(provider_test, GET_LOWER_TERM) {
  logger.init(addr);
  provider.start();
  usleep(3 * INTERVAL);
  EXPECT_CALL(logger, save_voted_for(addr));
  EXPECT_CALL(logger, save_current_term(1));
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  EXPECT_CALL(logger, save_voted_for(::testing::_)).Times(0);
  EXPECT_CALL(logger, save_current_term(::testing::_)).Times(0);
  append_entries_response r =
    append_entries(0, 0, 0, std::vector<raft_entry>(), 0, caddr);
  ASSERT_FALSE(r.is_success());
  ASSERT_EQ(r.get_term(), 1);
  ASSERT_EQ(fetch_state(), raft_state::leader);
}

TEST_F(provider_test, GET_LOWER_TERM_2) {
  logger.init(addr);
  provider.start();
  usleep(3 * INTERVAL);
  EXPECT_CALL(logger, save_voted_for(addr));
  EXPECT_CALL(logger, save_current_term(1));
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  EXPECT_CALL(logger, save_voted_for(::testing::_)).Times(0);
  EXPECT_CALL(logger, save_current_term(::testing::_)).Times(0);
  request_vote_response r = request_vote(0, caddr, 0, 0);
  ASSERT_FALSE(r.is_vote_granted());
  ASSERT_EQ(r.get_term(), 1);
  ASSERT_EQ(fetch_state(), raft_state::leader);
}

TEST_F(provider_test, NOT_FOUND_PREV_LOG) {
  logger.init(addr);
  provider.start();
  ASSERT_EQ(fetch_state(), raft_state::follower);
  EXPECT_CALL(logger, save_current_term(2));
  append_entries_response r =
    append_entries(2, 1, 1, std::vector<raft_entry>(), 0, caddr);
  ASSERT_EQ(r.get_term(), 2);
  ASSERT_FALSE(r.is_success());
  provider.run();
}

TEST_F(provider_test, CONFLICT_PREV_LOG) {
  logger.init(addr);
  provider.start();
  usleep(3 * INTERVAL);
  EXPECT_CALL(logger, save_voted_for(addr));
  EXPECT_CALL(logger, save_current_term(1));
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  EXPECT_CALL(logger,
              uuid_already_exists("046ccc3a-2dac-4e40-ae2e-76797a271fe2"));
  EXPECT_CALL(logger, append_log(1, "046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                                 "foo", "bar"));
  client_put_response r =
    client_put("046ccc3a-2dac-4e40-ae2e-76797a271fe2", "foo", "bar");
  ASSERT_EQ(r.get_error(), RAFT_SUCCESS);
  ASSERT_EQ(r.get_index(), 2);
  EXPECT_CALL(logger, save_current_term(2));
  append_entries_response r2 =
    append_entries(2, 2, 2, std::vector<raft_entry>(), 0, caddr);
  ASSERT_FALSE(r2.is_success());
  ASSERT_EQ(r2.get_term(), 2);
  ASSERT_EQ(fetch_state(), raft_state::follower);
}

TEST_F(provider_test, NOT_GRANTED_VOTE_WITH_LATE_LOG) {
  logger.init(addr);
  provider.start();
  usleep(3 * INTERVAL);
  EXPECT_CALL(logger, save_voted_for(addr));
  EXPECT_CALL(logger, save_current_term(1));
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  EXPECT_CALL(logger,
              uuid_already_exists("046ccc3a-2dac-4e40-ae2e-76797a271fe2"));
  EXPECT_CALL(logger, append_log(1, "046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                                 "foo", "bar"));
  client_put_response r =
    client_put("046ccc3a-2dac-4e40-ae2e-76797a271fe2", "foo", "bar");
  ASSERT_EQ(r.get_error(), RAFT_SUCCESS);
  ASSERT_EQ(r.get_index(), 2);
  EXPECT_CALL(logger, save_current_term(2));
  request_vote_response r2 = request_vote(2, caddr, 1, 0);
  ASSERT_FALSE(r2.is_vote_granted());
  ASSERT_EQ(r2.get_term(), 2);
  ASSERT_EQ(fetch_state(), raft_state::follower);
}

TEST_F(provider_test, NOT_GRANTED_VOTE_WITH_LATE_LOG_2) {
  logger.init(addr);
  provider.start();
  usleep(3 * INTERVAL);
  EXPECT_CALL(logger, save_voted_for(addr));
  EXPECT_CALL(logger, save_current_term(1));
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  EXPECT_CALL(logger,
              uuid_already_exists("046ccc3a-2dac-4e40-ae2e-76797a271fe2"));
  EXPECT_CALL(logger, append_log(1, "046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                                 "foo", "bar"));
  client_put_response r =
    client_put("046ccc3a-2dac-4e40-ae2e-76797a271fe2", "foo", "bar");
  ASSERT_EQ(r.get_error(), RAFT_SUCCESS);
  ASSERT_EQ(r.get_index(), 2);
  EXPECT_CALL(logger, save_current_term(2));
  request_vote_response r2 = request_vote(2, caddr, 1, 1);
  ASSERT_FALSE(r2.is_vote_granted());
  ASSERT_EQ(r2.get_term(), 2);
  ASSERT_EQ(fetch_state(), raft_state::follower);
}

TEST_F(provider_test, GRANTED_VOTE_WITH_LATEST_LOG) {
  logger.init(addr);
  provider.start();
  EXPECT_CALL(logger, save_voted_for(caddr));
  EXPECT_CALL(logger, save_current_term(1));
  request_vote_response r2 = request_vote(1, caddr, 1, 1);
  ASSERT_TRUE(r2.is_vote_granted());
  ASSERT_EQ(r2.get_term(), 1);
  ASSERT_EQ(fetch_state(), raft_state::follower);
}

TEST_F(provider_test, GRANTED_VOTE_WITH_LATEST_LOG_2) {
  logger.init(addr);
  provider.start();
  usleep(3 * INTERVAL);
  EXPECT_CALL(logger, save_voted_for(addr));
  EXPECT_CALL(logger, save_current_term(1));
  EXPECT_CALL(logger, append_log(1, ::testing::_, "__leader", "1"));
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  EXPECT_CALL(logger,
              uuid_already_exists("046ccc3a-2dac-4e40-ae2e-76797a271fe2"));
  EXPECT_CALL(logger, append_log(1, "046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                                 "foo", "bar"));
  client_put_response r =
    client_put("046ccc3a-2dac-4e40-ae2e-76797a271fe2", "foo", "bar");
  ASSERT_EQ(r.get_error(), RAFT_SUCCESS);
  ASSERT_EQ(r.get_index(), 2);
  EXPECT_CALL(logger, save_voted_for(caddr));
  EXPECT_CALL(logger, save_current_term(2));
  request_vote_response r2 = request_vote(2, caddr, 2, 1);
  ASSERT_TRUE(r2.is_vote_granted());
  ASSERT_EQ(r2.get_term(), 2);
  ASSERT_EQ(fetch_state(), raft_state::follower);
}

TEST_F(provider_test, GRANTED_VOTE_WITH_LATEST_LOG_3) {
  logger.init(addr);
  provider.start();
  usleep(3 * INTERVAL);
  EXPECT_CALL(logger, save_voted_for(addr));
  EXPECT_CALL(logger, save_current_term(1));
  EXPECT_CALL(logger, append_log(1, ::testing::_, "__leader", "1"));
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  EXPECT_CALL(logger,
              uuid_already_exists("046ccc3a-2dac-4e40-ae2e-76797a271fe2"));
  EXPECT_CALL(logger, append_log(1, "046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                                 "foo", "bar"));
  client_put_response r =
    client_put("046ccc3a-2dac-4e40-ae2e-76797a271fe2", "foo", "bar");
  ASSERT_EQ(r.get_error(), RAFT_SUCCESS);
  ASSERT_EQ(r.get_index(), 2);
  EXPECT_CALL(logger, save_voted_for(caddr));
  EXPECT_CALL(logger, save_current_term(2));
  request_vote_response r2 = request_vote(2, caddr, 0, 2);
  ASSERT_TRUE(r2.is_vote_granted());
  ASSERT_EQ(r2.get_term(), 2);
  ASSERT_EQ(fetch_state(), raft_state::follower);
}

TEST_F(provider_test, NOT_GRANTED_VOTE_WITH_ALREADY_VOTED) {
  logger.init(addr);
  provider.start();
  usleep(3 * INTERVAL);
  EXPECT_CALL(logger, save_voted_for(addr));
  EXPECT_CALL(logger, save_current_term(1));
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  request_vote_response r2 = request_vote(1, caddr, 0, 0);
  ASSERT_FALSE(r2.is_vote_granted());
  ASSERT_EQ(r2.get_term(), 1);
  ASSERT_EQ(fetch_state(), raft_state::leader);
}

TEST_F(provider_test, APPLY_ENTRIES) {
  logger.init(addr);
  provider.start();
  EXPECT_CALL(logger, save_log(1, 1, "046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                               "foo", "bar"));
  std::vector<raft_entry> ent;
  ent.emplace_back(1, 1, "046ccc3a-2dac-4e40-ae2e-76797a271fe2", "foo", "bar");
  append_entries_response r = append_entries(1, 0, 0, ent, 1, caddr);
  ASSERT_EQ(r.get_term(), 1);
  ASSERT_TRUE(r.is_success());
  usleep(3 * INTERVAL);
  EXPECT_CALL(logger, save_voted_for(addr));
  EXPECT_CALL(logger, save_current_term(2));
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  client_get_response r2 = client_get("foo");
  ASSERT_EQ(r2.get_error(), RAFT_SUCCESS);
  ASSERT_STREQ(r2.get_value().c_str(), "bar");
}

TEST_F(provider_test, NOT_DETERMINED_LEADER) {
  // dummy
  logger.init(addr + ",ofi+sockets://127.0.0.1:299999");
  provider.start();
  usleep(3 * INTERVAL);
  EXPECT_CALL(logger, save_voted_for(addr));
  EXPECT_CALL(logger, save_current_term(1));
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::candidate);
}

TEST_F(provider_test, BECOME_FOLLOWER_FROM_CANDIDATE) {
  logger.init(addr + ",ofi+sockets://127.0.0.1:299999");
  provider.start();
  usleep(3 * INTERVAL);
  EXPECT_CALL(logger, save_voted_for(addr));
  EXPECT_CALL(logger, save_current_term(1));
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::candidate);
  append_entries_response r =
    append_entries(1, 0, 0, std::vector<raft_entry>(), 0, caddr);
  ASSERT_TRUE(r.is_success());
  ASSERT_EQ(r.get_term(), 1);
  ASSERT_EQ(fetch_state(), raft_state::follower);
}

TEST_F(provider_test, CLIENT_GET_LEADER_NOT_FOUND) {
  logger.init(addr + ",ofi+sockets://127.0.0.1:299999");
  provider.start();
  ASSERT_EQ(fetch_state(), raft_state::follower);
  client_get_response r = client_get("hello");
  ASSERT_EQ(r.get_error(), RAFT_LEADER_NOT_FOUND);
  ASSERT_STREQ(r.get_value().c_str(), "");
  usleep(3 * INTERVAL);
  EXPECT_CALL(logger, save_voted_for(addr));
  EXPECT_CALL(logger, save_current_term(1));
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::candidate);
  client_get_response r2 = client_get("hello");
  ASSERT_EQ(r2.get_error(), RAFT_LEADER_NOT_FOUND);
  ASSERT_STREQ(r2.get_value().c_str(), "");
}

TEST_F(provider_test, CLIENT_PUT_LEADER_NOT_FOUND) {
  logger.init(addr + ",ofi+sockets://127.0.0.1:299999");
  provider.start();
  ASSERT_EQ(fetch_state(), raft_state::follower);
  client_put_response r =
    client_put("046ccc3a-2dac-4e40-ae2e-76797a271fe2", "foo", "bar");
  ASSERT_EQ(r.get_error(), RAFT_LEADER_NOT_FOUND);
  ASSERT_EQ(r.get_index(), 0);
  usleep(3 * INTERVAL);
  EXPECT_CALL(logger, save_voted_for(addr));
  EXPECT_CALL(logger, save_current_term(1));
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::candidate);
  client_put_response r2 =
    client_put("046ccc3a-2dac-4e40-ae2e-76797a271fe2", "foo", "bar");
  ASSERT_EQ(r2.get_error(), RAFT_LEADER_NOT_FOUND);
  ASSERT_EQ(r2.get_index(), 0);
}

TEST_F(provider_test, CANDIDATE_PERMANENTLY) {
  logger.init(addr + ",ofi+sockets://127.0.0.1:299999");
  provider.start();
  ASSERT_EQ(fetch_state(), raft_state::follower);
  usleep(3 * INTERVAL);
  EXPECT_CALL(logger, save_voted_for(addr));
  EXPECT_CALL(logger, save_current_term(1));
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::candidate);
  usleep(INTERVAL);
  EXPECT_CALL(logger, save_voted_for(addr));
  EXPECT_CALL(logger, save_current_term(2));
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::candidate);
  usleep(INTERVAL);
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::candidate);
}

TEST_F(provider_test, NODE_IS_NOT_LEADER) {
  logger.init(addr);
  provider.start();
  usleep(3 * INTERVAL);
  EXPECT_CALL(logger, save_voted_for(addr));
  EXPECT_CALL(logger, save_current_term(1));
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  EXPECT_CALL(logger, save_current_term(2));
  append_entries_response r =
    append_entries(2, 0, 0, std::vector<raft_entry>(), 0, caddr);
  ASSERT_TRUE(r.is_success());
  ASSERT_EQ(r.get_term(), 2);
  ASSERT_EQ(fetch_state(), raft_state::follower);
  client_get_response r2 = client_get("hello");
  ASSERT_EQ(r2.get_error(), RAFT_NODE_IS_NOT_LEADER);
  ASSERT_STREQ(r2.get_value().c_str(), "");
  ASSERT_STREQ(r2.get_leader_id().c_str(), caddr.c_str());
}

TEST_F(provider_test, PUT_INVALID_UUID) {
  logger.init(addr);
  provider.start();
  usleep(3 * INTERVAL);
  EXPECT_CALL(logger, save_voted_for(addr));
  EXPECT_CALL(logger, save_current_term(1));
  provider.run();
  ASSERT_EQ(fetch_state(), raft_state::leader);
  client_put_response r = client_put("foobarbuz", "foo", "bar");
  ASSERT_EQ(r.get_error(), RAFT_INVALID_UUID);
}

} // namespace