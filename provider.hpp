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

class raft_provider : public tl::provider<raft_provider> {
private:
  raft_state _state;
  raft_state get_state();
  void set_state(raft_state new_state);

  system_clock::time_point timeout_limit;
  void update_timeout_limit();

  // Mutex
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

public:
  raft_provider(tl::engine &e, raft_logger *logger, raft_fsm *fsm,
                uint16_t provider_id = RAFT_PROVIDER_ID);
  ~raft_provider();
  void run();
  void start();
  void finalize();
  bool remove_self_from_cluster();
};

#endif