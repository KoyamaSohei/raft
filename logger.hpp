#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <lmdb.h>

#include <string>
#include <thallium.hpp>

#include "types.hpp"

class raft_logger {
protected:
  std::string id;

public:
  raft_logger(std::string _id) : id(_id){};
  virtual ~raft_logger(){};

  virtual void init(std::string addrs) = 0;
  virtual void get_nodes_from_buf(std::string buf,
                                  std::vector<std::string> &nodes) = 0;
  virtual void get_buf_from_nodes(std::string &buf,
                                  std::vector<std::string> nodes) = 0;
  virtual void bootstrap_state_from_log(int &current_term,
                                        std::string &voted_for,
                                        std::vector<std::string> &nodes) = 0;
  virtual void save_current_term(int current_term) = 0;
  virtual void save_voted_for(std::string voted_for) = 0;
  virtual void get_log(int index, int &term, std::string &uuid,
                       std::string &key, std::string &value) = 0;
  virtual void save_log(int index, int term, std::string uuid, std::string key,
                        std::string value) = 0;
  // append_log returns index
  virtual int append_log(int term, std::string uuid, std::string key,
                         std::string value) = 0;
  virtual int get_term(int index) = 0;
  virtual void get_last_log(int &index, int &term) = 0;
  virtual bool match_log(int index, int term) = 0;
  virtual bool uuid_already_exists(std::string uuid) = 0;
  virtual std::string generate_uuid() = 0;
};

class lmdb_raft_logger : public raft_logger {
private:
  MDB_env *env;
  // 永続Stateのうち、_current_termと_voted_forと最新のclusterを保存するDB
  const char *state_db = "state_db";
  MDB_val current_term_key = {13 * sizeof(char), (void *)"current_term"};
  MDB_val voted_for_key = {10 * sizeof(char), (void *)"voted_for"};
  MDB_val cluster_key = {8 * sizeof(char), (void *)"cluster"};
  // 永続Stateのうち、log[]を保存するDB
  const char *log_db = "log_db";
  // uuidを保存するDB
  const char *uuid_db = "uuid_db";
  // 保存されているlogの要素数
  int stored_log_num;
  std::string get_log_str(int index);
  void save_uuid(std::string uuid, MDB_txn *ptxn = NULL);
  int get_uuid(std::string uuid, MDB_txn *ptxn = NULL);
  void save_log_str(int index, std::string log_str, MDB_txn *ptxn = NULL);

public:
  lmdb_raft_logger(std::string id);
  ~lmdb_raft_logger();

  void init(std::string addrs);
  void get_nodes_from_buf(std::string buf, std::vector<std::string> &nodes);
  void get_buf_from_nodes(std::string &buf, std::vector<std::string> nodes);
  void bootstrap_state_from_log(int &current_term, std::string &voted_for,
                                std::vector<std::string> &nodes);
  void save_current_term(int current_term);
  void save_voted_for(std::string voted_for);
  void get_log(int index, int &term, std::string &uuid, std::string &key,
               std::string &value);
  void save_log(int index, int term, std::string uuid, std::string key,
                std::string value);
  // append_log returns index
  int append_log(int term, std::string uuid, std::string key,
                 std::string value);
  int get_term(int index);
  void get_last_log(int &index, int &term);
  bool match_log(int index, int term);
  bool uuid_already_exists(std::string uuid);
  std::string generate_path(std::string id);
  std::string generate_uuid();
};

#endif