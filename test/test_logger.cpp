#include <dirent.h>
#include <gtest/gtest.h>
#include <stdio.h>

#include "../logger.hpp"

#define ADDR "127.0.0.1:30000"

namespace {
class logger_test : public ::testing::Test {
protected:
  logger_test() {}
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
  raft_logger logger("sockets://" ADDR);
  DIR* dir = opendir("log-" ADDR);
  if (dir) {
    closedir(dir);
  } else {
    ADD_FAILURE();
  }
}

TEST_F(logger_test, OFI_TCP_SUPPORT) {
  raft_logger logger("ofi+tcp;ofi_rxm://" ADDR);
  DIR* dir = opendir("log-" ADDR);
  if (dir) {
    closedir(dir);
  } else {
    ADD_FAILURE();
  }
}

TEST_F(logger_test, OFI_SOCKETS_SUPPORT) {
  raft_logger logger("ofi+sockets://" ADDR);
  DIR* dir = opendir("log-" ADDR);
  if (dir) {
    closedir(dir);
  } else {
    ADD_FAILURE();
  }
}

TEST_F(logger_test, SET_DUMMY) {
  raft_logger logger("sockets://" ADDR);
  int i, t;
  logger.get_last_log(i, t);
  ASSERT_EQ(i, 0);
  ASSERT_EQ(t, 0);
}

TEST_F(logger_test, DUMMY_IS_HELLO) {
  raft_logger logger("sockets://" ADDR);
  int index = 0;
  int term;
  std::string key, value;
  logger.get_log(index, term, key, value);
  ASSERT_EQ(term, 0);
  ASSERT_STREQ(key.c_str(), "hello");
  ASSERT_STREQ(value.c_str(), "world");
}

TEST_F(logger_test, APPEND_LOG) {
  raft_logger logger("sockets://" ADDR);
  int idx = logger.append_log(1, "foo", "bar");
  ASSERT_EQ(idx, 1);
  int term;
  std::string key, value;
  logger.get_log(idx, term, key, value);
  ASSERT_EQ(term, 1);
  ASSERT_STREQ(key.c_str(), "foo");
  ASSERT_STREQ(value.c_str(), "bar");
}

TEST_F(logger_test, BOOTSTRAP_FROM_EMPTY) {
  raft_logger logger("sockets://" ADDR);
  int current_term;
  std::string voted_for;
  logger.bootstrap_state_from_log(current_term, voted_for);
  ASSERT_EQ(current_term, 0);
  ASSERT_STREQ(voted_for.c_str(), "");
}

TEST_F(logger_test, BOOTSTRAP) {
  raft_logger logger("sockets://" ADDR);
  logger.save_current_term(1);
  logger.save_voted_for("sockets://127.0.0.1:12345");
  int term;
  std::string voted_for;
  logger.bootstrap_state_from_log(term, voted_for);
  ASSERT_EQ(term, 1);
  ASSERT_STREQ(voted_for.c_str(), "sockets://127.0.0.1:12345");
}

} // namespace

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}