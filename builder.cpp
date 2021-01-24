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

void get_vector_from_seq(std::vector<std::string> &vec,
                         const std::string &seq) {
  vec.clear();
  if (seq.empty()) return;

  std::string::size_type pos = 0, next;

  do {
    next = seq.find(",", pos);
    if (next - pos) { vec.emplace_back(seq.substr(pos, next - pos)); }
    pos = next + 1;
  } while (next != std::string::npos);
}

void get_seq_from_vector(std::string &seq,
                         const std::vector<std::string> &vec) {
  seq.clear();
  for (std::string item : vec) {
    seq += item;
    seq += ",";
  }
  seq.pop_back();
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