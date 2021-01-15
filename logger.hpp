#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <lmdb.h>
#include <string>
#include <thallium.hpp>
#include "types.hpp"


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
  std::string get_log_str(int index);
  void save_log_str(int index,std::string log_str,MDB_txn *ptxn=NULL);
public:
  raft_logger(tl::endpoint id);
  ~raft_logger();
  void bootstrap_state_from_log(int &current_term,std::string &voted_for);
  void save_current_term(int current_term);
  void save_voted_for(std::string voted_for);
  void get_log(int index,int &term,std::string &key,std::string &value);
  // append_log returns index
  int append_log(int term,std::string key,std::string value);
  void get_last_log(int &index,int &term);
};

#endif