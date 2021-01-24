#include "fsm.hpp"

#include <json/json.h>

#include <cassert>

kvs_raft_fsm::kvs_raft_fsm() : raft_fsm() {
  data["hello"] = "world";
}

kvs_raft_fsm::~kvs_raft_fsm() {}

void kvs_raft_fsm::apply(std::string command) {

  if (command.empty()) { return; }

  Json::CharReaderBuilder builder;
  Json::Value root;
  JSONCPP_STRING err_str;
  const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());

  int ok = reader->parse(command.c_str(), command.c_str() + command.length(),
                         &root, &err_str);
  if (!ok) {
    printf("apply failed, command %s is invalid\n", command.c_str());
    return;
  }

  std::string key = root["key"].asString();
  std::string value = root["value"].asString();

  data[key] = value;
}

std::string kvs_raft_fsm::resolve(std::string query) {
  return data[query];
}