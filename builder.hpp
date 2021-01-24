#ifndef BUILDER_HPP
#define BUILDER_HPP

#include <string>
#include <vector>

/**
 *  generate uuid, uuid length is UUID_LENGTH
 *  @param uuid dst e.g.) "046ccc3a-2dac-4e40-ae2e-76797a271fe2"
 */
void generate_uuid(std::string &uuid);

/**
 *  parse key value command
 *  @param key key  e.g.) "foo"
 *  @param value value e.g.) "bar"
 *  @param src src e.g.) "{\"key\":\"foo\",\"value\":\"bar\"}"
 */
void parse_command(std::string &key, std::string &value,
                   const std::string &src);

/**
 *  build key value command
 *  @param dst dst e.g.) "{\"key\":\"foo\",\"value\":\"bar\"}"
 *  @param key key  e.g.) "foo"
 *  @param value value e.g.) "bar"
 */
void build_command(std::string &dst, const std::string &key,
                   const std::string &value);

/**
 *  parse string sequence. sequence splits string with ','
 *  @param vec dst e.g.) ["127.0.0.1","127.0.0.2"]
 *  @param seq src e.g.) "127.0.0.1,127.0.0.2"
 */
void get_vector_from_seq(std::vector<std::string> &vec, const std::string &seq);

/**
 *  parse string sequence. sequence splits string with ','
 *  @param seq dst e.g.) "127.0.0.1,127.0.0.2"
 *  @param vec src e.g.) ["127.0.0.1","127.0.0.2"]
 */
void get_seq_from_vector(std::string &seq, const std::vector<std::string> &vec);

/**
 *  parse log from string.
 *  @param term     dst e.g.) 2
 *  @param uuid     dst e.g.) "046ccc3a-2dac-4e40-ae2e-76797a271fe2"
 *  @param command  dst e.g.) "{\"key\":\"foo\",\"value\":\"bar\"}"
 *  @param src      src , which is build with `build_log`
 */
void parse_log(int &term, std::string &uuid, std::string &command,
               const std::string &src);

/**
 *  build log from string.
 *  @param dst      dst which can be parsed with `parse_log`
 *  @param term     src e.g.) 2
 *  @param uuid     src e.g.) "046ccc3a-2dac-4e40-ae2e-76797a271fe2"
 *  @param command  src e.g.) "{\"key\":\"foo\",\"value\":\"bar\"}"
 */
void build_log(std::string &dst, const int &term, const std::string &uuid,
               const std::string &command);

#endif