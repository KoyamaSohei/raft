#include "logger.hpp"

#include <dirent.h>
#include <gtest/gtest.h>
#include <stdio.h>

#include "builder.hpp"

#define ADDR "127.0.0.1:30000"

namespace {
class logger_test : public ::testing::Test {
protected:
  lmdb_raft_logger logger;
  logger_test() : logger(ADDR, raft_logger_mode::init) {}

  ~logger_test() { logger.clean_up(); }
};

TEST_F(logger_test, EXIST_DIR) {
  DIR* dir = opendir("log-" ADDR);
  if (dir) {
    closedir(dir);
  } else {
    ADD_FAILURE();
  }
}

TEST_F(logger_test, SET_DUMMY) {
  int i, t;
  logger.get_last_log(i, t);
  ASSERT_EQ(i, 1);
  ASSERT_EQ(t, 0);
}

TEST_F(logger_test, DUMMY_IS_CLUSTER_INFO) {
  int index = 1;
  int term;
  std::string uuid, command;
  logger.get_log(index, term, uuid, command);
  ASSERT_EQ(term, 0);
  int p, n;
  std::set<std::string> pn, nn;
  parse_conf_log(p, pn, n, nn, command);
  ASSERT_EQ(p, 0);
  ASSERT_EQ(n, 1);
}

TEST_F(logger_test, APPEND_LOG) {
  int idx = logger.append_log("046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                              "{\"key\":\"foo\",\"value\":\"bar\"}");
  ASSERT_EQ(idx, 2);
  int term;
  std::string uuid, command;
  logger.get_log(idx, term, uuid, command);
  ASSERT_EQ(term, 0);
  ASSERT_STREQ(uuid.c_str(), "046ccc3a-2dac-4e40-ae2e-76797a271fe2");
  ASSERT_STREQ(command.c_str(), "{\"key\":\"foo\",\"value\":\"bar\"}");
}

TEST_F(logger_test, MATCHLOG) {
  ASSERT_TRUE(logger.match_log(0, 0));
}

TEST_F(logger_test, MATCHLOG_NOTFOUND) {
  ASSERT_FALSE(logger.match_log(1234, 0));
}

TEST_F(logger_test, UUID_ALREADY_EXISTS) {
  int idx = logger.append_log("046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                              "{\"key\":\"foo\",\"value\":\"bar\"}");
  ASSERT_EQ(idx, 2);
  ASSERT_TRUE(logger.contains_uuid("046ccc3a-2dac-4e40-ae2e-76797a271fe2"));
  ASSERT_FALSE(logger.contains_uuid("146ccc3a-2dac-4e40-ae2e-76797a271fe2"));
}

TEST_F(logger_test, CONFLICT_UUID) {
  int idx = logger.append_log("046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                              "{\"key\":\"foo\",\"value\":\"bar\"}");
  ASSERT_EQ(idx, 2);
  ASSERT_DEATH(logger.append_log("046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                                 "{\"key\":\"foo\",\"value\":\"bar\"}");
               , "");
}

TEST_F(logger_test, CONFLICT_UUID_2) {
  int idx = logger.append_log("046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                              "{\"key\":\"foo\",\"value\":\"bar\"}");
  ASSERT_EQ(idx, 2);
  ASSERT_DEATH(logger.append_log("046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                                 "{\"key\":\"foo\",\"value\":\"bar\"}");
               , "");
}

TEST_F(logger_test, VOTED_FOR) {
  logger.set_voted_for("foo");
  ASSERT_TRUE(logger.exists_voted_for());
}

TEST_F(logger_test, VOTED_FOR_CLEAR) {
  logger.set_voted_for("foo");
  ASSERT_TRUE(logger.exists_voted_for());
  logger.set_current_term(2);
  ASSERT_FALSE(logger.exists_voted_for());
}

TEST_F(logger_test, VOTED_FOR_SELF) {
  logger.set_voted_for_self();
  ASSERT_TRUE(logger.exists_voted_for());
}

TEST_F(logger_test, CURRENT_TERM) {
  logger.set_current_term(100);
  ASSERT_EQ(logger.get_current_term(), 100);
}

TEST_F(logger_test, GET_PEER) {
  ASSERT_EQ(logger.get_peers().size(), 0);
}

TEST_F(logger_test, NUM_NODES) {
  ASSERT_EQ(logger.get_num_nodes(), 1);
}

TEST_F(logger_test, LAST_LOG_APPLIED) {
  ASSERT_EQ(logger.get_last_conf_applied(), 1);
}

TEST_F(logger_test, RECOVER) {
  std::string addr = "127.0.0.1:8888";

  auto run1 = [&]() {
    lmdb_raft_logger logger2(addr, raft_logger_mode::init);
    printf("logger2 init\n");
    logger2.append_log("046ccc3a-2dac-4e40-ae2e-76797a271fe2", "foo-bar-buz");
  };

  auto run2 = [&]() {
    lmdb_raft_logger logger3(addr, raft_logger_mode::bootstrap);
    printf("logger3 bootstrap\n");
    int i, t;
    logger3.get_last_log(i, t);
    ASSERT_EQ(i, 2);
    ASSERT_EQ(t, 0);
    std::string uuid, cmd;
    logger3.get_log(i, t, uuid, cmd);
    ASSERT_STREQ(uuid.c_str(), "046ccc3a-2dac-4e40-ae2e-76797a271fe2");
    ASSERT_STREQ(cmd.c_str(), "foo-bar-buz");
    logger3.clean_up();
  };
  printf("run1\n");
  run1();
  usleep(INTERVAL);
  printf("run2\n");
  run2();
}

} // namespace
