#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <lmdb.h>
#include <string>
#include <thallium.hpp>

namespace tl = thallium;

class raft_logger {
private:
  MDB_env *env;
  // 永続Stateのうち、_current_termと_voted_forを保存するDB
  const char *state_db = "state_db";
  MDB_val current_term_key = { 13*sizeof(char), (void *)"current_term" };
  MDB_val voted_for_key    = { 10*sizeof(char), (void *)"voted_for" };
  // 永続Stateのうち、log[]を保存するDB
  const char *log_db = "log_db";
  // 保存されているlogの要素数
  int stored_log_num;
public:
  raft_logger(tl::endpoint id);
  ~raft_logger();
  void bootstrap_state_from_log(int &current_term,std::string &voted_for);
  void save_current_term(int current_term);
  void save_voted_for(std::string voted_for);
  void save_log(int index,std::string log_str);
  std::string get_log(int index);
};

#endif