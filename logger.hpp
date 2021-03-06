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
 *  @param mode     logger mode. please see init() join() and bootstrap()
 *                  child class of raft_logger must
 *                  - implements init(),join(),bootstrap()
 *                  - call init(),join(),bootstrap() on contructor
 */
class raft_logger {
private:
  /**
   * init initialize DB and Persistent State
   * if want nodes to join exists cluster, please use join() instead.
   * location of log is depends on id.
   * if want to restart with past log, please use bootstrap() instread.
   * if log already exists, aborted.
   */
  virtual void init() = 0;

  /**
   *  join initialize DB and Persistent State
   *  if want to start new cluster, please use init() instead.
   *  location of log is depends on id.
   *  if want to restart with past log, please use bootstrap() instread.
   *  if log already exists, aborted.
   */
  virtual void join() = 0;

  /**
   * bootstrap recover DB and Persistent State
   * location of log is depends on id.
   * if log already exists, all states
   * (voted_for,current_term,nodes,peers,and stored_log_num,last_conf_applied)
   * will be overwritten with past log
   * if log is empty, aborted.
   */
  virtual void bootstrap() = 0;

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
   * peers is subset of nodes. peers must NOT contains self id.
   */
  std::set<std::string> peers;

  /**
   * num of stored log[]. this is used for append log
   * if call append_log when stored_log_num=N,
   * index of new log will be N
   * Note: index=0's log is empty,and commited from the beginning.
   * if node start with init(), index=1's log is special entry
   * which has cluster info, and cluster consists of only self node.
   */
  int stored_log_num;

  /**
   * latest index of config change.
   */
  int last_conf_applied;

  /**
   *  parse log from string.
   *  @param term     dst e.g.) 2
   *  @param uuid     dst e.g.) "046ccc3a-2dac-4e40-ae2e-76797a271fe2"
   *  @param command  dst e.g.) "{\"key\":\"foo\",\"value\":\"bar\"}"
   *  @param src      src , which is build with `build_log`
   */
  static void parse_log(int &term, std::string &uuid, std::string &command,
                        const std::string &src);

  /**
   *  build log from string.
   *  @param dst      dst which can be parsed with `parse_log`
   *  @param term     src e.g.) 2
   *  @param uuid     src e.g.) "046ccc3a-2dac-4e40-ae2e-76797a271fe2"
   *  @param command  src e.g.) "{\"key\":\"foo\",\"value\":\"bar\"}"
   */
  static void build_log(std::string &dst, const int &term,
                        const std::string &uuid, const std::string &command);

  /**
   *  parse cluster change config log from this src.
   *  this special log is stored at `log_db` as a part of entry.
   *  and this src also stored at `state_db` (to find  more efficiently)
   *  @param prev_index index on which prev cluster change commited
   *  @param prev_nodes nodes which belog to prev cluster
   *  @param next_index index on which next cluster change applied
   *  @param next_nodes nodes which belog to next cluster
   *  @param src        src string
   */
  static void parse_conf_log(int &prev_index, std::set<std::string> &prev_nodes,
                             int &next_index, std::set<std::string> &next_nodes,
                             const std::string &src);

  /**
   *  build cluster change config log to dst
   *  this special log is stored at `log_db` as a part of entry.
   *  and this dst also stored at `state_db` (to find  more efficiently)
   *  @param dst        dst string
   *  @param prev_index index on which prev cluster change commited
   *  @param prev_nodes nodes which belog to prev cluster
   *  @param next_index index on which next cluster change applied
   *  @param next_nodes nodes which belog to next cluster
   */
  static void build_conf_log(std::string &dst, const int &prev_index,
                             const std::set<std::string> &prev_nodes,
                             const int &next_index,
                             const std::set<std::string> &next_nodes);

public:
  raft_logger(std::string _id, raft_logger_mode mode)
    : id(_id), voted_for(""), current_term(0), stored_log_num(0){};
  /**
   * get_id returns self id
   * @return self id
   *
   */
  virtual std::string get_id() = 0;

  /**
   * get_peers get nodes in cluster without self.
   * @return peers
   */
  virtual std::set<std::string> &get_peers() = 0;

  /**
   * get_num_nodes return number of nodes(include self)
   * @return  number of nodes
   */
  virtual int get_num_nodes() = 0;

  /**
   * contains_self_in_nodes check if node contain self id.
   * if false, this node may disrupt other servers
   * so we have to shutdown as soon as possible.
   * @return if node contain self id.
   */
  virtual bool contains_self_in_nodes() = 0;

  /**
   * get_current_term return current term.
   * @return current_term
   */
  virtual int get_current_term() = 0;

  /**
   * set_current_term save current term with new_term.
   * @param new_term
   */
  virtual void set_current_term(int new_term) = 0;

  /**
   * exists_voted_for check if already voted in this term.
   * when call set_current_term, voted_for is cleared automatically.
   *
   * @return check if already voted in this term.
   */
  virtual bool exists_voted_for() = 0;

  /**
   * set_voted_for_self update voted_for with self id
   */
  virtual void set_voted_for_self() = 0;

  /**
   * set_voted_for update voted_for with new_addr
   * @param new_addr want to vote for
   */
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
   * setadd_conf_log saves special entry to log.
   * @param new_server server which will be added into cluster.
   * @return index log index
   */
  virtual int set_add_conf_log(const std::string &new_server) = 0;

  /**
   * set_remove_conf_log saves special entry to log.
   * @param old_server server which will be removed from cluster.
   * @return index log index
   */
  virtual int set_remove_conf_log(const std::string &old_server) = 0;

  virtual ~raft_logger(){};

  /**
   * clean_up clean up log.
   * use this before restart node with initial state.
   * usually, this method is used for testing.
   */
  virtual void clean_up() = 0;
};

/**
 * lmdb_raft_logger is one of raft_logger implements, and depends on LMDB
 * LMDB has 2 databases, log_db and state_db
 *
 * 1. log_db
 * log_db store log entry.
 * log entry consists of
 *   - index (:int)
 *   - term (:int)
 *   - uuid (:string)
 *   - command (:string)
 * index is primary key.
 * term is used for log matching in raft.
 *   Note: According to https://raft.github.io/raft.pdf §5.3,
 *   if two logs contain an entry with the same index and term,
 *   then the logs are identical in all entries up through the given index.
 * uuid is used to prevent duplicate requests
 *   Note: if uuid is special, (special means that uuid begins '77777777-')
 *   this log entry is special entry
 *   special entry has cluster info (see 2. state_db)
 * command is used for fsm,see fsm.hpp
 *
 * 2. state_db
 * state_db store persistent state like below
 *  - current_term (:int)
 *  - voted_for (:string)
 *  - last_conf_applied (:int)
 *
 */
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

  void init();
  void join();
  void bootstrap();

public:
  lmdb_raft_logger(std::string _id, raft_logger_mode mode);
  ~lmdb_raft_logger();

  void clean_up();

  std::string get_id();
  std::set<std::string> &get_peers();
  int get_num_nodes();
  bool contains_self_in_nodes();

  int get_current_term();
  void set_current_term(int current_term);

  bool exists_voted_for();
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

  int set_add_conf_log(const std::string &new_server);

  int set_remove_conf_log(const std::string &old_server);
};

#endif