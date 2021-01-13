#ifndef PROVIDER_HPP
#define PROVIDER_HPP

#include <thallium.hpp>
#include <chrono>
#include <vector>
#include <string>
#include "types.hpp"

#define INTERVAL 1
#define TIMEOUT  3

namespace tl = thallium;
using system_clock = std::chrono::system_clock;

enum class raft_state {
  follower,
  candidate,
  leader,
};

class raft_provider : public tl::provider<raft_provider> {
private:
  // Node ID
  tl::endpoint id;
  // 現在の状態(follower/candidate/leader)
  raft_state _state;
  // timeout_limit < 現在の時刻 であれば followerからcandidateへ遷移
  system_clock::time_point timeout_limit;
  void update_timeout_limit();
  // nodes;
  int num_nodes;
  std::vector<tl::endpoint> nodes;
  // Mutex
  tl::mutex mu;
  raft_state get_state();
  void set_state(raft_state new_state);
  // current Term
  int _current_term;
  int get_current_term();
  // Voted endpoint on this term
  tl::endpoint _voted_for;
  // append_entries_rpc
  append_entries_response append_entries_rpc(append_entries_request &req);
  tl::remote_procedure m_append_entries_rpc;
  // request_vote rpc
  request_vote_response request_vote_rpc(request_vote_request &req);
  tl::remote_procedure m_request_vote_rpc;
  // ---- rpc def end   ---
  void become_follower();
  void run_follower();

  void become_candidate();
  void run_candidate();

  void become_leader();
  void run_leader();
public:
  raft_provider(tl::engine& e,uint16_t provider_id=1);
  ~raft_provider();
  void run();
  void append_node(std::string addr);
};

#endif