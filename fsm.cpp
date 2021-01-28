#include "fsm.hpp"

#include <cassert>
#include <cereal/archives/json.hpp>
#include <cereal/cereal.hpp>
#include <cereal/types/set.hpp>

#include "builder.hpp"

kvs_raft_fsm::kvs_raft_fsm() : raft_fsm() {
  data["hello"] = "world";
}

kvs_raft_fsm::~kvs_raft_fsm() {}

void kvs_raft_fsm::parse_command(std::string &key, std::string &value,
                                 const std::string &src) {
  std::stringstream ss;
  { ss << src; }
  {
    cereal::JSONInputArchive archive(ss);
    archive(CEREAL_NVP(key), (value));
  }
}

void kvs_raft_fsm::build_command(std::string &dst, const std::string &key,
                                 const std::string &value) {
  std::ostringstream ss;
  {
    cereal::JSONOutputArchive archive(ss);
    archive(CEREAL_NVP(key), CEREAL_NVP(value));
  }
  dst = ss.str();
}

void kvs_raft_fsm::apply(std::string command) {

  if (command.empty()) { return; }

  std::string key, value;
  parse_command(key, value, command);

  data[key] = value;
}

std::string kvs_raft_fsm::resolve(std::string query) {
  return data[query];
}