#include "fsm.hpp"

#include <cassert>

#include "builder.hpp"

kvs_raft_fsm::kvs_raft_fsm() : raft_fsm() {
  data["hello"] = "world";
}

kvs_raft_fsm::~kvs_raft_fsm() {}

void kvs_raft_fsm::apply(std::string command) {

  if (command.empty()) { return; }

  std::string key, value;
  parse_command(key, value, command);

  data[key] = value;
}

std::string kvs_raft_fsm::resolve(std::string query) {
  return data[query];
}