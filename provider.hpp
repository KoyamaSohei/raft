#ifndef PROVIDER_HPP
#define PROVIDER_HPP

#include <chrono>
#include <map>
#include <string>
#include <thallium.hpp>
#include <vector>

#include "fsm.hpp"
#include "logger.hpp"
#include "types.hpp"

/**
 * raft_provider handle rpc and manages node state.
 * raft_provider depends on thallium::provider
 * refer: https://mochi.readthedocs.io/en/latest/thallium/09_providers.html
 * for detail about raft
 * refer: https://raft.github.io/raft.pdf
 *
 */
class raft_provider : public tl::provider<raft_provider> {
private:
  /**
   * _state is node state.
   * By Section5.1 Raft Basis,
   * > At any given time each server is in one of three states:
   *   leader, follower, or candidate.
   * please DO NOT use this except for get_state() or set_state()
   */
  raft_state _state;

  /**
   * get_state() get _state.
   * before call get_state(),
   * please call mu.lock() and enter critical section.
   * @return raft_state
   */
  raft_state get_state();

  /**
   * get_state() set _state.
   * before call set_state(),
   * please call mu.lock() and enter critical section.
   * @param new_state
   */
  void set_state(raft_state new_state);

  /**
   * timeout_limit is used for 2 ways.
   * for detail,
   * please refer page 4 "rules for servers"
   * in https://raft.github.io/raft.pdf
   *
   * 1. in follower state,
   *    if current time > timeout_limit,
   *    - become candidate
   *    - increment current term
   *    - start election
   * 2. in candidate state,
   *    if current time > timeout_limit,
   *    - become candidate again
   *    - increment current term
   *    - start election again
   */
  system_clock::time_point timeout_limit;

  /**
   * update_timeout_limit is called
   * - before become follower
   * - before become candidate
   * - after recerive entry from append entries rpc
   */
  void update_timeout_limit();

  /**
   *  last_entry_recerived handles the letest time
   *  which append entries rpc received on.
   *  in follower state,
   *  if received request vote rpc but
   *  current time - last_entry_recerived < TIMEOUT,
   *  NOT granted vote for that request.
   *  because recently received rpc from current leader.
   *  this is used for prevent to make orphan node to leader.
   *  for detail, please refer "section 4.2.3 disruptive server"
   *  in https://github.com/ongardie/dissertation/blob/master/book.pdf
   *
   *  Note exception:
   *  if request vote rpc has has_disrupt_permission flag,
   *  even if current time < timeout_limit,
   *  grant vote
   *  (of cause,candidate log must be valid for become leader)
   *
   *  this exception is used for leadership transfer extension.
   *  for detail,please refer
   *  - "section 3.10 Leadership transfer extension"
   *  - "section 4.2.3 disruptive server"
   *  in https://github.com/ongardie/dissertation/blob/master/book.pdf
   */
  system_clock::time_point last_entry_recerived;

  /**
   * mu is mutex.
   * please call mu.lock() before access ANY variables,state,etc.
   * also, mu.lock() is called beginning run() and
   * mu.unlock() is called ending run().
   * this is to avoid changing the state in the middle of the run().
   * please mu.unlock() before call ANY rpc and
   * mu.lock() again if rpc ended.
   * this is to avoid waiting rpc response in critical section.
   */
  tl::mutex mu;

  /**
   * cond is condition_variable.
   * this is used for waiting the log is commited.
   * provider calls cond.notify_all() at the end of run()
   */
  tl::condition_variable cond;

  /**
   * set_force_current_term set term
   * @param term
   */
  void set_force_current_term(int term);

  /**
   * logger manages Persistent State.
   * this is injected during provider initialization.
   * for detail, please refer logger.hpp
   */
  raft_logger *logger;

  /**
   * fsm manages Finite State Machine.
   * this is injected during provider initialization.
   * for detail, please refer fsm.hpp
   */
  raft_fsm *fsm;

  /**
   * By Section5.4 Safety,
   * commit_index is index of highest log entry
   * known to be committed.
   * initialized to 0, increases monotonically.
   * if a log entry is committed in a
   * given term, then that entry will be present in the logs
   * of the leaders for all higher-numbered terms.
   * please DO NOT use this except for
   * get_commit_index() or set_commit_index()
   */
  int _commit_index;

  /**
   * get_commit_index get commit index.
   * @return commit index
   */
  int get_commit_index();

  /**
   * set_commit_index set commit index.
   * @param index
   */
  void set_commit_index(int index);

  /**
   * By Raft Basis,
   * > last_applied is index of highest log entry applied to state
   * machine (initialized to 0, increases monotonically)
   * please DO NOT use this except for
   * get_last_applied() or set_last_applied()
   */
  int _last_applied;

  /**
   * get_last_applied get last applied index.
   * @return last applied index
   */
  int get_last_applied();

  /**
   * set_last_applied set last applied index.
   * @param index
   */
  void set_last_applied(int index);

  /**
   * for each server (in this case, that server is called "node")
   * _next_index is index of the next log entry to send to node
   * please DO NOT use this except for
   * get_next_index() or set_next_index()
   */
  std::map<std::string, int> _next_index;

  /**
   * get_next_index get next index of node.
   * @param node
   * @return next index of node
   */
  int get_next_index(const std::string &node);

  /**
   * set_next_index set next index of node
   * @param node
   * @param index
   */
  void set_next_index(const std::string &node, int index);

  /**
   * reset_next_index reset all next index.
   * this is called after election finished.
   */
  void reset_next_index();

  /**
   * for each server (in this case, that server is called "node")
   * _match_index is index of highest log entryknown to be replicated on node.
   * please DO NOT use this except for
   * get_match_index() or set_match_index()
   */
  std::map<std::string, int> _match_index;

  /**
   * get_match_index get match index of node
   * @param node
   * @return match index of node
   */
  int get_match_index(const std::string &node);

  /**
   * set_match_index set match index of node
   * @param node
   * @param index
   */
  void set_match_index(const std::string &node, int index);

  /**
   * reset_match_index reset all match index.
   * this is called after election finished.
   */
  void reset_match_index();

  /**
   * leader_hint is setted when receriving append_entries_rpc
   * this is used for tell client now leader.
   * by this, client can find leader more efficiently.
   */
  std::string leader_hint;

  // cache to resolve host
  std::map<std::string, tl::provider_handle> _node_to_handle;
  tl::provider_handle &get_handle(const std::string &node);
  void reset_handle(const std::string &node);

  // append_entries_rpc
  void append_entries_rpc(const tl::request &req, int req_term,
                          int req_prev_index, int req_prev_term,
                          const std::vector<raft_entry> &req_entries,
                          int req_leader_commit,
                          const std::string &req_leader_id);
  tl::remote_procedure m_append_entries_rpc;

  // request_vote rpc
  void request_vote_rpc(const tl::request &req, int req_term,
                        const std::string &req_candidate_id,
                        int req_last_log_index, int req_last_log_term,
                        bool has_disrupt_permission);
  tl::remote_procedure m_request_vote_rpc;

  // timeout_now rpc
  void timeout_now_rpc(const tl::request &req, int req_term, int req_prev_index,
                       int req_prev_term);
  tl::remote_procedure m_timeout_now_rpc;

  // client request rpc
  void client_request_rpc(const tl::request &req, const std::string &uuid,
                          const std::string &command);
  tl::remote_procedure m_client_request_rpc;

  // client query rpc
  void client_query_rpc(const tl::request &req, const std::string &query);
  tl::remote_procedure m_client_query_rpc;

  // add server rpc
  void add_server_rpc(const tl::request &req, const std::string &new_server);
  tl::remote_procedure m_add_server_rpc;

  // remove server rpc
  void remove_server_rpc(const tl::request &req, const std::string &old_server);
  tl::remote_procedure m_remove_server_rpc;

  // echo state rpc (for debug)
  void echo_state_rpc(const tl::request &req);

  void become_follower();
  void run_follower();

  void become_candidate(bool has_disrupt_permission = false);
  void run_candidate();

  void become_leader();
  void run_leader();

  void transfer_leadership();
  void finalize();

  static void tick_loop(void *provider);

public:
  raft_provider(tl::engine &e, raft_logger *logger, raft_fsm *fsm,
                uint16_t provider_id = RAFT_PROVIDER_ID);
  ~raft_provider();
  void run();
  void wait_add_self_into_cluster(std::string target_hint);
  bool remove_self_from_cluster();
};
#endif