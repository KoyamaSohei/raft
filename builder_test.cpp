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

} // namespace