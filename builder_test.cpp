#include "builder.hpp"

#include <gtest/gtest.h>

namespace {

TEST(test_get_seq_from_set, EMPTY) {
  std::set<std::string> src;
  std::string seq;
  get_seq_from_set(seq, src);
  ASSERT_STREQ(seq.c_str(), "");
}

TEST(test_get_seq_from_set, ONE) {
  std::set<std::string> src{"127.0.0.1:30000"};
  std::string seq;
  get_seq_from_set(seq, src);
  ASSERT_STREQ(seq.c_str(), "127.0.0.1:30000");
}

TEST(test_get_seq_from_set, TWO) {
  std::set<std::string> src{"127.0.0.1:30000", "127.0.0.1:30001"};
  std::string seq;
  get_seq_from_set(seq, src);
  ASSERT_STREQ(seq.c_str(), "127.0.0.1:30000,127.0.0.1:30001");
}

TEST(test_get_seq_from_set, THREE) {
  std::set<std::string> src{"127.0.0.1:30000", "127.0.0.1:30001",
                            "127.0.0.1:30002"};
  std::string seq;
  get_seq_from_set(seq, src);
  ASSERT_STREQ(seq.c_str(), "127.0.0.1:30000,127.0.0.1:30001,127.0.0.1:30002");
}

TEST(test_get_seq_from_set, THREE_ORDER) {
  std::set<std::string> src{"127.0.0.1:30001", "127.0.0.1:30002",
                            "127.0.0.1:30000"};
  std::string seq;
  get_seq_from_set(seq, src);
  ASSERT_STREQ(seq.c_str(), "127.0.0.1:30000,127.0.0.1:30001,127.0.0.1:30002");
}

TEST(test_get_seq_from_set, THREE_ORDER_2) {
  std::set<std::string> src;
  src.insert("127.0.0.1:30002");
  src.insert("127.0.0.1:30000");
  src.insert("127.0.0.1:30001");

  std::string seq;
  get_seq_from_set(seq, src);
  ASSERT_STREQ(seq.c_str(), "127.0.0.1:30000,127.0.0.1:30001,127.0.0.1:30002");
}

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