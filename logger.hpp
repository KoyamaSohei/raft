#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <lmdb.h>

#include <set>
#include <string>
#include <thallium.hpp>

#include "types.hpp"

/**
 *  raft_logger manages Persistent State.
 *  @param id       suffix of server address.  e.g 127.0.0.1:30000
 *  @param nodes    id of clusters. nodes must contains self id.
 *
 */
class raft_logger {
protected:
  /**
   * id is suffix of server address.
   * e.g 127.0.0.1:30000
   * raft_provider binds this id
   */
  std::string id;

  /**
   * voted_for is candidate id that received vote in current term
   * (or empty if none)
   */
  std::string voted_for;

  /**
   * current_term is latest term server has seen
   * (initialized to 0 on first boot, increases monotonically)
   */
  int current_term;

  /**
   * nodes is id of clusters. nodes must contains self id.
   */
  std::set<std::string> nodes;

  /**
   * peers is subset of nodes . peers must NOT contains self id.
   */
  std::set<std::string> peers;

  /**
   * num of stored log[]. this is used for append log
   * if call append_log when stored_log_num=N,
   * index of new log will be N
   * Note: index=0's log is nodes info,and commited from the beginning.
   */
  int stored_log_num;

  /**
   * latest index of config change.
   */
  int last_conf_applied;

public:
  raft_logger(std::string _id, std::set<std::string> _nodes)
    : id(_id)
    , voted_for("")
    , current_term(0)
    , nodes(_nodes)
    , peers(_nodes)
    , stored_log_num(0) {
    peers.erase(id);
  };
  virtual ~raft_logger(){};

  /**
   * init initialize DB and Persistent State
   * location of log is depends on id.
   * if log already exists, all states
   * (voted_for,current_term,nodes,peers,and stored_log_num,last_conf_applied)
   * will be overwritten with past log
   */
  virtual void init() = 0;

  /**
   * clean_up clean up log.
   * use this before restart node with initial state.
   */
  virtual void clean_up() = 0;

  virtual std::string get_id() = 0;
  virtual std::set<std::string> &get_peers() = 0;
  virtual int get_num_nodes() = 0;

  virtual int get_current_term() = 0;
  virtual void set_current_term(int new_term) = 0;

  virtual bool exists_voted_for() = 0;
  virtual void clear_voted_for() = 0;
  virtual void set_voted_for_self() = 0;
  virtual void set_voted_for(const std::string &new_addr) = 0;

  /**
   * get_log get log with index (in other words, key is index)
   * @param index   key
   * @param term
   * @param uuid
   * @param command
   */
  virtual void get_log(const int index, int &term, std::string &uuid,
                       std::string &command) = 0;
  /**
   * set_log saves log into DB
   * @param index
   * @param term
   * @param uuid
   * @param command
   */
  virtual void set_log(const int index, const int term, const std::string &uuid,
                       const std::string &command) = 0;

  /**
   * append_log saves log into DB, behind latest log
   * @return index
   * @param uuid
   * @param command
   */
  virtual int append_log(const std::string &uuid,
                         const std::string &command) = 0;

  /**
   * get_term returns term of the log which has log.index = `index`
   * @return term
   * @param index
   */
  virtual int get_term(const int index) = 0;

  /**
   * get_last_log get latest log
   * @param index
   * @param term
   */
  virtual void get_last_log(int &index, int &term) = 0;

  /**
   * match_log check if same log was saved already.
   *
   * Note:
   * log1.index == log2.index && log1.term == log2.term => log1 == log2
   * @param index
   * @param term
   */
  virtual bool match_log(const int index, const int term) = 0;

  /**
   * contains_uuid check if same request was saved already.
   * @param uuid
   * @return true if log contains uuid already
   */
  virtual bool contains_uuid(const std::string &uuid) = 0;

  /**
   * config change is consist of special entry (which uuid is special)
   * logger have to do below
   * 1. tell provider that latest index of config change
   *    leader prevent new config change if old change was not commited
   * 2. if special entry save to log,
   *    - update nodes,peers,last_conf_applied
   *    - save this entry to state db
   * 3. if normal entry overwrite special entry,
   *    - rollback nodes,peers,last_conf_applied
   *    - save prev special entry to state db
   */

  /**
   * get_last_conf_applied returns latest index of config change.
   * @return latest index of config change.
   */
  virtual int get_last_conf_applied() = 0;

  /**
   * set_remove_conf_log saves special entry to log.
   * @param term term
   * @param uuid special uuid
   * @param old_server server which will be removed from cluster.
   */
  virtual void set_remove_conf_log(const int &term, const std::string &uuid,
                                   const std::string &old_server) = 0;
};

class lmdb_raft_logger : public raft_logger {
private:
  MDB_env *env;
  const char *state_db = "state_db";

  MDB_val current_term_key = {13 * sizeof(char), (void *)"current_term"};
  MDB_val voted_for_key = {10 * sizeof(char), (void *)"voted_for"};
  MDB_val last_conf_applied_key = {18 * sizeof(char),
                                   (void *)"last_conf_applied"};

  const char *log_db = "log_db";
  const char *uuid_db = "uuid_db";

  /**
   * get_log_str get log from LMDB
   * @param index
   * @param ptxn parent transaction
   * @return log (stringified json)
   */
  std::string get_log_str(int index, MDB_txn *ptxn = NULL);

  /**
   * set_log_str save log into LMDB
   * @param index
   * @param log_str stringified json which want to save
   * @param ptxn parent transaction
   */
  void set_log_str(int index, std::string log_str, MDB_txn *ptxn = NULL);

  /**
   * set_uuid save uuid into LMDB
   * @param uuid uuid
   * @param ptxn parent transaction
   */
  void set_uuid(std::string uuid, MDB_txn *ptxn = NULL);

  /**
   * get_uuid get uuid
   * @param uuid uuid
   * @param ptxn parent transaction
   * @return success
   */
  bool get_uuid(const std::string &uuid, MDB_txn *ptxn = NULL);

public:
  lmdb_raft_logger(std::string _id, std::set<std::string> _nodes);
  ~lmdb_raft_logger();

  void init();

  void clean_up();

  std::string get_id();
  std::set<std::string> &get_peers();
  int get_num_nodes();

  int get_current_term();
  void set_current_term(int current_term);

  bool exists_voted_for();
  void clear_voted_for();
  void set_voted_for_self();
  void set_voted_for(const std::string &new_addr);

  void get_log(const int index, int &term, std::string &uuid,
               std::string &command);
  void set_log(const int index, const int term, const std::string &uuid,
               const std::string &command);

  int append_log(const std::string &uuid, const std::string &command);

  int get_term(const int index);

  void get_last_log(int &index, int &term);

  bool match_log(const int index, const int term);
  bool contains_uuid(const std::string &uuid);

  int get_last_conf_applied();

  void set_remove_conf_log(const int &term, const std::string &uuid,
                           const std::string &old_server);
};

#endif