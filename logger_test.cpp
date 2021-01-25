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
  logger_test() : logger(ADDR, std::set<std::string>{ADDR}) { logger.init(); }

  ~logger_test() {
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
  ASSERT_EQ(i, 0);
  ASSERT_EQ(t, 0);
}

TEST_F(logger_test, DUMMY_IS_CLUSTER_INFO) {
  int index = 0;
  int term;
  std::string uuid, command;
  logger.get_log(index, term, uuid, command);
  ASSERT_EQ(term, 0);
  int p, n;
  std::set<std::string> pn, nn;
  parse_conf_log(p, pn, n, nn, command);
  std::string pn_buf, nn_buf;
  get_seq_from_set(pn_buf, pn);
  get_seq_from_set(nn_buf, nn);
  ASSERT_EQ(p, 0);
  ASSERT_EQ(n, 0);

  ASSERT_STREQ(pn_buf.c_str(), ADDR);
  ASSERT_STREQ(nn_buf.c_str(), ADDR);
}

TEST_F(logger_test, APPEND_LOG) {
  int idx = logger.append_log("046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                              "{\"key\":\"foo\",\"value\":\"bar\"}");
  ASSERT_EQ(idx, 1);
  int term;
  std::string uuid, command;
  logger.get_log(idx, term, uuid, command);
  ASSERT_EQ(term, 0);
  ASSERT_STREQ(uuid.c_str(), "046ccc3a-2dac-4e40-ae2e-76797a271fe2");
  ASSERT_STREQ(command.c_str(), "{\"key\":\"foo\",\"value\":\"bar\"}");
}

TEST_F(logger_test, BOOTSTRAP_FROM_EMPTY) {
  int current_term;
  std::string voted_for;
  std::set<std::string> nodes;
  ASSERT_EQ(current_term, 0);
  ASSERT_STREQ(voted_for.c_str(), "");
  ASSERT_EQ((int)nodes.size(), 1);
  ASSERT_STREQ(nodes.begin()->c_str(), ADDR);
}

TEST_F(logger_test, BOOTSTRAP) {
  logger.set_current_term(1);
  logger.set_voted_for("127.0.0.1:12345");
  int term;
  ASSERT_EQ(term, 1);
  ASSERT_TRUE(logger.exists_voted_for());
  ASSERT_EQ(logger.get_num_nodes(), 1);
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
  ASSERT_EQ(idx, 1);
  ASSERT_TRUE(logger.contains_uuid("046ccc3a-2dac-4e40-ae2e-76797a271fe2"));
  ASSERT_FALSE(logger.contains_uuid("146ccc3a-2dac-4e40-ae2e-76797a271fe2"));
}

TEST_F(logger_test, CONFLICT_UUID) {
  int idx = logger.append_log("046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                              "{\"key\":\"foo\",\"value\":\"bar\"}");
  ASSERT_EQ(idx, 1);
  ASSERT_DEATH(logger.append_log("046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                                 "{\"key\":\"foo\",\"value\":\"bar\"}");
               , "");
}

TEST_F(logger_test, CONFLICT_UUID_2) {
  int idx = logger.append_log("046ccc3a-2dac-4e40-ae2e-76797a271fe2",
                              "{\"key\":\"foo\",\"value\":\"bar\"}");
  ASSERT_EQ(idx, 1);
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
  logger.clear_voted_for();
  ASSERT_FALSE(logger.exists_voted_for());
}

TEST_F(logger_test, VOTED_FOR_SELF) {
  logger.set_voted_for_self();
  ASSERT_TRUE(logger.exists_voted_for());
}

} // namespace
