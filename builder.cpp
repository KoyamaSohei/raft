#include "builder.hpp"

#include <json/json.h>
#include <uuid/uuid.h>

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

void parse_command(std::string &key, std::string &value,
                   const std::string &src) {
  Json::CharReaderBuilder builder;
  Json::Value root;
  Json::String err_str;
  const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
  int ok =
    reader->parse(src.c_str(), src.c_str() + src.length(), &root, &err_str);
  if (!ok) {
    printf("%s\n", err_str.c_str());
    abort();
  }
  key.assign(root["key"].asString());
  value.assign(root["value"].asString());
}

void build_command(std::string &dst, const std::string &key,
                   const std::string &value) {
  Json::Value root;
  Json::StreamWriterBuilder builder;
  root["key"] = key;
  root["value"] = value;
  dst.assign(Json::writeString(builder, root));
}

void get_set_from_seq(std::set<std::string> &dst, const std::string &src) {
  dst.clear();
  if (src.empty()) return;

  std::string::size_type pos = 0, next;

  do {
    next = src.find(",", pos);
    if (next - pos) { dst.insert(src.substr(pos, next - pos)); }
    pos = next + 1;
  } while (next != std::string::npos);
}

void get_seq_from_set(std::string &dst, const std::set<std::string> &src) {
  dst.clear();
  for (std::string item : src) {
    dst += item;
    dst += ",";
  }
  dst.pop_back();
}

void parse_log(int &term, std::string &uuid, std::string &command,
               const std::string &src) {
  Json::CharReaderBuilder builder;
  Json::Value root;
  Json::String err_str;
  const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
  int ok =
    reader->parse(src.c_str(), src.c_str() + src.length(), &root, &err_str);
  if (!ok) {
    printf("%s\n", err_str.c_str());
    abort();
  }
  term = root["term"].asInt();
  uuid.assign(root["uuid"].asString());
  command.assign(root["command"].asString());
}

void build_log(std::string &dst, const int &term, const std::string &uuid,
               const std::string &command) {
  Json::Value root;
  Json::StreamWriterBuilder builder;
  root["term"] = term;
  root["uuid"] = uuid;
  root["command"] = command;
  dst.assign(Json::writeString(builder, root));
}

void parse_conf_log(int &prev_index, std::set<std::string> &prev_nodes,
                    int &next_index, std::set<std::string> &next_nodes,
                    const std::string &src) {
  Json::CharReaderBuilder builder;
  Json::Value root;
  Json::String err_str;
  const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
  int ok =
    reader->parse(src.c_str(), src.c_str() + src.length(), &root, &err_str);
  if (!ok) {
    printf("%s\n", err_str.c_str());
    abort();
  }
  prev_index = root["prev_index"].asInt();
  next_index = root["next_index"].asInt();
  std::string prev_str(root["prev_nodes"].asString());
  std::string next_str(root["next_nodes"].asString());
  get_set_from_seq(prev_nodes, prev_str);
  get_set_from_seq(next_nodes, next_str);
}

void build_conf_log(std::string &dst, const int &prev_index,
                    const std::set<std::string> &prev_nodes,
                    const int &next_index,
                    const std::set<std::string> &next_nodes) {
  Json::Value root;
  Json::StreamWriterBuilder builder;
  std::string prev_str, next_str;

  get_seq_from_set(prev_str, prev_nodes);
  get_seq_from_set(next_str, next_nodes);

  root["prev_index"] = prev_index;
  root["prev_nodes"] = prev_str;
  root["next_index"] = next_index;
  root["next_nodes"] = next_str;

  dst.assign(Json::writeString(builder, root));
}