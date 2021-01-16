#ifndef PROVIDER_HPP
#define PROVIDER_HPP

#include <thallium.hpp>
#include <chrono>
#include <vector>
#include <string>
#include <map>
#include "types.hpp"
#include "logger.hpp"
#include "kvs.hpp"

class raft_provider : public tl::provider<raft_provider> {
private:
  // Node ID
  tl::endpoint id;
  // Leader ID
  tl::provider_handle leader_id;
  // 現在の状態(follower/candidate/leader)
  raft_state _state;
  // follower時タイムアウト -> canditateに遷移、election開始
  // candidate時タイムアウト -> candidateに遷移、election再トライ
  system_clock::time_point timeout_limit;
  void update_timeout_limit();
  // nodes;
  int num_nodes;
  std::vector<std::string> nodes;
  std::map<std::string,tl::provider_handle> node_to_handle;
  // Mutex
  tl::mutex mu;
  // THIS MUST BE CALLED IN CRITICAL SECTION
  raft_state get_state();
  // THIS MUST BE CALLED IN CRITICAL SECTION
  void set_state(raft_state new_state);
  // current Term
  // SAVE TO LOGGER BEFORE CHANGE (Write Ahead Log)
  int _current_term;
  // THIS MUST BE CALLED IN CRITICAL SECTION
  int get_current_term();
  // If RPC request or response contains term T > currentTerm: set currentTerm = T, convert to follower
  // THIS MUST BE CALLED IN CRITICAL SECTION
  void set_force_current_term(int term);
  // Voted endpoint on this term
  // SAVE TO LOGGER BEFORE CHANGE (Write Ahead Log)
  std::string _voted_for;
  // logger
  raft_logger logger;
  // kvs
  raft_kvs kvs;
  // for each server, index of the next log entryto send to that server
  std::map<std::string,int> next_index;
  // for each server, index of highest log entryknown to be replicated on server
  std::map<std::string,int> match_index;
  // index of highest log entry known to be committed
  int _commit_index;
  int get_commit_index();
  void set_commit_index(int index);
  // append_entries_rpc
  append_entries_response append_entries_rpc(append_entries_request &req);
  tl::remote_procedure m_append_entries_rpc;
  // request_vote rpc
  request_vote_response request_vote_rpc(request_vote_request &req);
  tl::remote_procedure m_request_vote_rpc;
  // client put rpc
  int client_put_rpc(std::string key,std::string value);
  tl::remote_procedure m_client_put_rpc;
  // client get rpc
  client_get_response client_get_rpc(std::string key);
  tl::remote_procedure m_client_get_rpc;
  // echo state rpc
  int echo_state_rpc();
  void become_follower();
  void run_follower();

  void become_candidate();
  void run_candidate();

  void become_leader();
  void run_leader();

  // ノードを追加 ready時にのみ呼び出し可能
  void append_node(std::string addr);
public:
  raft_provider(tl::engine& e,uint16_t provider_id=RAFT_PROVIDER_ID);
  ~raft_provider();
  void run();
  // readyからfollowerに遷移
  void start(std::vector<std::string> &addrs);
};

#endif