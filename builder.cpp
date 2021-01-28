#include "builder.hpp"

#include <uuid/uuid.h>

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