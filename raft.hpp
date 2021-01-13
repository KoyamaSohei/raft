#ifndef RAFT_HPP
#define RAFT_HPP

#include <thallium.hpp>
#include "types.hpp"

namespace tl = thallium;
using system_clock = std::chrono::system_clock;

enum class raft_state {
  follower,
  candidate,
  leader,
};

class raft_provider : public tl::provider<raft_provider> {
private:
  // 現在の状態(follower/candidate/leader)
  raft_state state;
  // 最後に append_entries_rpc を受け取った時刻
  system_clock::time_point last_entry_recerived;

  // append_entries_rpc
  append_entries_response append_entries_rpc(append_entries_request &req);
  tl::remote_procedure m_append_entries_rpc;

  // ---- rpc def end   ---
  void run_follower();

  void become_candidate();
  void run_candidate();

  void run_leader();
public:
  raft_provider(tl::engine& e,uint16_t provider_id=1);
  ~raft_provider();
  void run();
};

#endif