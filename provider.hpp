#ifndef PROVIDER_HPP
#define PROVIDER_HPP

#include <chrono>
#include <map>
#include <string>
#include <thallium.hpp>
#include <vector>

#include "kvs.hpp"
#include "logger.hpp"
#include "types.hpp"

class raft_provider : public tl::provider<raft_provider> {
private:
  // Node ID
  std::string id;
  // Leader ID
  std::string leader_id;
  // 現在の状態(follower/candidate/leader)
  raft_state _state;
  // follower時タイムアウト -> canditateに遷移、election開始
  // candidate時タイムアウト -> candidateに遷移、election再トライ
  system_clock::time_point timeout_limit;
  void update_timeout_limit();
  // nodes;
  int num_nodes;
  std::vector<std::string> nodes;
  std::map<std::string, tl::provider_handle> node_to_handle;
  // Mutex for _state,_current_term,_commit_index
  tl::mutex mu;
  // Cond for client_put reply after the log was commited.
  tl::condition_variable cond;
  // THIS MUST BE CALLED IN CRITICAL SECTION
  raft_state get_state();
  // THIS MUST BE CALLED IN CRITICAL SECTION
  void set_state(raft_state new_state);
  // current Term
  // SAVE TO LOGGER BEFORE CHANGE (Write Ahead Log)
  int _current_term;
  // THIS MUST BE CALLED IN CRITICAL SECTION
  int get_current_term();
  // If RPC request or response contains term T > currentTerm: set currentTerm =
  // T, convert to follower
  // THIS MUST BE CALLED IN CRITICAL SECTION
  void set_force_current_term(int term);
  // Voted endpoint on this term
  // SAVE TO LOGGER BEFORE CHANGE (Write Ahead Log)
  std::string _voted_for;
  // logger
  raft_logger *logger;
  // kvs
  raft_kvs kvs;
  // for each server, index of the next log entryto send to that server
  std::map<std::string, int> next_index;
  // for each server, index of highest log entryknown to be replicated on server
  std::map<std::string, int> match_index;
  // index of highest log entry known to be committed
  int _commit_index;
  int get_commit_index();
  void set_commit_index(int index);
  // append_entries_rpc
  void append_entries_rpc(const tl::request &r, int req_term,
                          int req_prev_index, int req_prev_term,
                          std::vector<raft_entry> req_entries,
                          int req_leader_commit, std::string req_leader_id);
  tl::remote_procedure m_append_entries_rpc;
  // request_vote rpc
  void request_vote_rpc(const tl::request &r, int req_term,
                        std::string req_candidate_id, int req_last_log_index,
                        int req_last_log_term);
  tl::remote_procedure m_request_vote_rpc;
  // client put rpc
  void client_put_rpc(const tl::request &r, std::string uuid, std::string key,
                      std::string value);
  tl::remote_procedure m_client_put_rpc;
  // client get rpc
  void client_get_rpc(const tl::request &r, std::string key);
  tl::remote_procedure m_client_get_rpc;
  // echo state rpc
  void echo_state_rpc(const tl::request &r);
  void become_follower();
  void run_follower();

  void become_candidate();
  void run_candidate();

  void become_leader();
  void run_leader();

public:
  raft_provider(tl::engine &e, raft_logger *logger, std::string _id,
                uint16_t provider_id = RAFT_PROVIDER_ID);
  ~raft_provider();
  void run();
  void start();
  void finalize();
};

#endif