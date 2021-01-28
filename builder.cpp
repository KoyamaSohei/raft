#include "builder.hpp"

#include <uuid/uuid.h>

#include <cereal/archives/json.hpp>
#include <cereal/cereal.hpp>
#include <cereal/types/set.hpp>
#include <sstream>

#include "types.hpp"

void generate_uuid(std::string &uuid) {
  uuid_t id;
  uuid_generate(id);
  char sid[UUID_LENGTH];
  uuid_unparse_lower(id, sid);
  uuid.assign(sid);
}

void generate_special_uuid(std::string &uuid) {
  generate_uuid(uuid);
  for (int k = 0; k < 8; k++) { uuid[k] = '7'; }
}

bool uuid_is_special(const std::string &uuid) {
  bool yes = true;
  for (int k = 0; k < 8; k++) { yes &= uuid[k] == '7'; }
  return yes;
}

void get_set_from_seq(std::set<std::string> &dst, const std::string &src) {
  dst.clear();
  std::string buffer;
  for (char c : src) {
    if (c == ',') {
      if (buffer.empty()) continue;
      dst.insert(buffer);
      buffer.clear();
      continue;
    }
    buffer.push_back(c);
  }
  if (buffer.empty()) return;
  dst.insert(buffer);
}

void parse_log(int &term, std::string &uuid, std::string &command,
               const std::string &src) {
  std::stringstream ss;
  { ss << src; }
  {
    cereal::JSONInputArchive archive(ss);
    archive(CEREAL_NVP(term), CEREAL_NVP(uuid), CEREAL_NVP(command));
  }
}

void build_log(std::string &dst, const int &term, const std::string &uuid,
               const std::string &command) {
  std::stringstream ss;
  {
    cereal::JSONOutputArchive archive(ss);
    archive(CEREAL_NVP(term), CEREAL_NVP(uuid), CEREAL_NVP(command));
  }
  dst = ss.str();
}

void parse_conf_log(int &prev_index, std::set<std::string> &prev_nodes,
                    int &next_index, std::set<std::string> &next_nodes,
                    const std::string &src) {
  std::stringstream ss;
  { ss << src; }
  {
    cereal::JSONInputArchive archive(ss);
    archive(CEREAL_NVP(prev_index), CEREAL_NVP(prev_nodes),
            CEREAL_NVP(next_index), CEREAL_NVP(next_nodes));
  }
}

void build_conf_log(std::string &dst, const int &prev_index,
                    const std::set<std::string> &prev_nodes,
                    const int &next_index,
                    const std::set<std::string> &next_nodes) {
  std::stringstream ss;
  {
    cereal::JSONOutputArchive archive(ss);
    archive(CEREAL_NVP(prev_index), CEREAL_NVP(prev_nodes),
            CEREAL_NVP(next_index), CEREAL_NVP(next_nodes));
  }
  dst = ss.str();
}