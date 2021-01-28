#ifndef BUILDER_HPP
#define BUILDER_HPP

#include <set>
#include <string>

/**
 *  generate uuid, uuid length is UUID_LENGTH
 *  @param uuid dst e.g.) "046ccc3a-2dac-4e40-ae2e-76797a271fe2"
 */
void generate_uuid(std::string &uuid);

/**
 *  generate special uuid.
 *  this uuid used with special(e.g. cluster config change) log entry.
 *  and log content is NOT a command which fsm can apply.
 *  if uuid is special, fsm don't apply that entry.
 *  @param uuid dst e.g.) "77777777-2dac-4e40-ae2e-76797a271fe2"
 */

void generate_special_uuid(std::string &uuid);

/**
 *  check if uuid is special.
 *  @param uuid dst e.g.) "77777777-2dac-4e40-ae2e-76797a271fe2"
 */
bool uuid_is_special(const std::string &uuid);

#endif