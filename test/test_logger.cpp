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

} // namespace

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}