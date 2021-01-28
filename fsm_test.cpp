#include "fsm.hpp"

#include <gtest/gtest.h>

#include "builder.hpp"

namespace {

TEST(kvs_fsm_test, DUMMY) {
  kvs_raft_fsm fsm;
  ASSERT_EQ(fsm.resolve("hello"), "world");
}

TEST(kvs_fsm_test, APPLY) {
  kvs_raft_fsm fsm;
  fsm.apply("{\"key\":\"foo\",\"value\":\"bar\"}");
  ASSERT_EQ(fsm.resolve("foo"), "bar");
}

TEST(kvs_fsm_test, APPLY_2) {
  kvs_raft_fsm fsm;
  fsm.apply("{\n\t\"key\" : \"__cluster\",\n\t\"value\" : \"127.0.0.1\"\n}");
  ASSERT_EQ(fsm.resolve("__cluster"), "127.0.0.1");
}

TEST(kvs_fsm_test, APPLY_3) {
  kvs_raft_fsm fsm;
  fsm.apply(
    "{          \n\t\"key\" :       \"__cluster\",\n   \t\"value\":    "
    "\"127.0.0.1\"   \n}");
  ASSERT_EQ(fsm.resolve("__cluster"), "127.0.0.1");
}

TEST(kvs_fsm_test, KVS_BUILD_COMMAND) {
  kvs_raft_fsm fsm;
  std::string command;
  kvs_raft_fsm::build_command(command, "foo", "bar");
  fsm.apply(command);
  ASSERT_EQ(fsm.resolve("foo"), "bar");
}

TEST(kvs_fsm_test, APPLY_EMPTY) {
  kvs_raft_fsm fsm;
  fsm.apply("");
}

} // namespace