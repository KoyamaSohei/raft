#include "builder.hpp"

#include <gtest/gtest.h>

namespace {

TEST(test_get_set_from_seq, EMPTY) {
  std::set<std::string> dst;
  std::string src;
  get_set_from_seq(dst, src);
  ASSERT_EQ(dst.size(), 0);
}

TEST(test_get_set_from_seq, ONE) {
  std::set<std::string> dst;
  std::string src = "127.0.0.1:30000";
  get_set_from_seq(dst, src);
  ASSERT_EQ(dst.size(), 1);
  ASSERT_STREQ(dst.begin()->c_str(), src.c_str());
}

TEST(test_get_set_from_seq, TWO) {
  std::set<std::string> dst;
  std::string src = "127.0.0.1:30000,127.0.0.1:30001";
  get_set_from_seq(dst, src);
  ASSERT_EQ(dst.size(), 2);
  ASSERT_STREQ(dst.begin()->c_str(), "127.0.0.1:30000");
  ASSERT_STREQ(dst.rbegin()->c_str(), "127.0.0.1:30001");
}

TEST(test_get_set_from_seq, THREE) {
  std::set<std::string> dst;
  std::string src = "127.0.0.1:30000,127.0.0.1:30001,127.0.0.1:30002";
  get_set_from_seq(dst, src);
  ASSERT_EQ(dst.size(), 3);
  ASSERT_STREQ(dst.begin()->c_str(), "127.0.0.1:30000");
  ASSERT_STREQ(dst.rbegin()->c_str(), "127.0.0.1:30002");
}

TEST(test_get_set_from_seq, THREE_ORDER) {
  std::set<std::string> dst;
  std::string src = "127.0.0.1:30001,127.0.0.1:30000,127.0.0.1:30002";
  get_set_from_seq(dst, src);
  ASSERT_EQ(dst.size(), 3);
  ASSERT_STREQ(dst.begin()->c_str(), "127.0.0.1:30000");
  ASSERT_STREQ(dst.rbegin()->c_str(), "127.0.0.1:30002");
}

} // namespace