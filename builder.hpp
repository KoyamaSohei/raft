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
 *  @param dst dst e.g.) {"127.0.0.1","127.0.0.2"}
 *  @param src src e.g.) "127.0.0.1,127.0.0.2"
 */
void get_set_from_seq(std::set<std::string> &dst, const std::string &src);

/**
 *  parse string sequence. sequence splits string with ','
 *  @param dst dst e.g.) "127.0.0.1,127.0.0.2"
 *  @param src src e.g.) {"127.0.0.1","127.0.0.2"}
 */
void get_seq_from_set(std::string &dst, const std::set<std::string> &src);

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

/**
 *  parse cluster change config log from this src.
 *  this special log is stored at `log_db` as a part of entry.
 *  key = SPECIAL_ENTRY_KEY, value = {this src}
 *  and this src also stored at `state_db` (to find  more efficiently)
 *  @param prev_index index on which prev cluster change commited
 *  @param prev_nodes nodes which belog to prev cluster
 *  @param next_index index on which next cluster change applied
 *  @param next_nodes nodes which belog to next cluster
 *  @param src        src string
 */
void parse_conf_log(int &prev_index, std::set<std::string> &prev_nodes,
                    int &next_index, std::set<std::string> &next_nodes,
                    const std::string &src);

/**
 *  build cluster change config log to dst
 *  this special log is stored at `log_db` as a part of entry.
 *  key = SPECIAL_ENTRY_KEY, value = {this src}
 *  and this dst also stored at `state_db` (to find  more efficiently)
 *  @param dst        dst string
 *  @param prev_index index on which prev cluster change commited
 *  @param prev_nodes nodes which belog to prev cluster
 *  @param next_index index on which next cluster change applied
 *  @param next_nodes nodes which belog to next cluster
 */
void build_conf_log(std::string &dst, const int &prev_index,
                    const std::set<std::string> &prev_nodes,
                    const int &next_index,
                    const std::set<std::string> &next_nodes);

#endif