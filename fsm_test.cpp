#include "fsm.hpp"

#include <gtest/gtest.h>
#include <json/json.h>

namespace {

TEST(fsm_test, DUMMY) {
  kvs_raft_fsm fsm;
  ASSERT_EQ(fsm.resolve("hello"), "world");
}

TEST(fsm_test, APPLY) {
  kvs_raft_fsm fsm;
  fsm.apply("{\"key\":\"foo\",\"value\":\"bar\"}");
  ASSERT_EQ(fsm.resolve("foo"), "bar");
}

TEST(fsm_test, APPLY_2) {
  kvs_raft_fsm fsm;
  fsm.apply("{\n\t\"key\" : \"__cluster\",\n\t\"value\" : \"127.0.0.1\"\n}");
  ASSERT_EQ(fsm.resolve("__cluster"), "127.0.0.1");
}

TEST(fsm_test, APPLY_3) {
  kvs_raft_fsm fsm;
  fsm.apply(
    "{          \n\t\"key\" :       \"__cluster\",\n   \t\"value\":    "
    "\"127.0.0.1\"   \n}");
  ASSERT_EQ(fsm.resolve("__cluster"), "127.0.0.1");
}

TEST(fsm_test, JSON_BUILDER) {
  kvs_raft_fsm fsm;
  Json::Value root;
  Json::StreamWriterBuilder builder;
  root["key"] = "foo";
  root["value"] = "bar";
  std::string command = Json::writeString(builder, root);
  fsm.apply(command);
  ASSERT_EQ(fsm.resolve("foo"), "bar");
}

TEST(fsm_test, JSON_BUILDER_2) {
  kvs_raft_fsm fsm;
  Json::Value root, value;
  Json::StreamWriterBuilder wbuilder;
  Json::CharReaderBuilder rbuilder;
  value["hello"] = "world";
  value["year"] = 2021;
  std::string str = Json::writeString(wbuilder, value);
  root["key"] = "foo";
  root["value"] = str;
  std::string command = Json::writeString(wbuilder, root);
  fsm.apply(command);
  ASSERT_EQ(fsm.resolve("foo"), str);
  JSONCPP_STRING err_str;
  const std::unique_ptr<Json::CharReader> reader(rbuilder.newCharReader());

  ASSERT_TRUE(
    reader->parse(str.c_str(), str.c_str() + str.length(), &root, &err_str));
  ASSERT_STREQ(root["hello"].asString().c_str(), "world");
  ASSERT_EQ(root["year"].asInt(), 2021);
}

TEST(fsm_test, APPLY_EMPTY) {
  kvs_raft_fsm fsm;
  fsm.apply("");
}

} // namespace