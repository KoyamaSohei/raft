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

  tl::condition_variable cond;

  void set_force_current_term(int term);

  // logger
  raft_logger *logger;
  // fsm
  raft_fsm *fsm;

  // index of highest log entry known to be committed
  int _commit_index;
  int get_commit_index();
  void set_commit_index(int index);

  // last applied
  int _last_applied;
  int get_last_applied();
  void set_last_applied(int index);

  // for each server, index of the next log entryto send to that server
  std::map<std::string, int> _next_index;
  int get_next_index(const std::string &node);
  void set_next_index(const std::string &node, int index);

  // for each server, index of highest log entryknown to be replicated on server
  std::map<std::string, int> _match_index;
  int get_match_index(const std::string &node);
  void set_match_index(const std::string &node, int index);

  // use this to tell client
  std::string leader_hint;

  // cache to resolve host
  std::map<std::string, tl::provider_handle> _node_to_handle;
  tl::provider_handle &get_handle(const std::string &node);

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
  void wait_add_self_into_cluster();
  bool remove_self_from_cluster();
};
#endif