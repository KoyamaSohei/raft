#ifndef RAFT_HPP
#define RAFT_HPP

#include <thallium.hpp>

namespace tl = thallium;

enum class raft_state {
  follower,
  candidate,
  leader,
};

class raft_provider : public tl::provider<raft_provider> {
private:
  // 現在の状態(follower/candidate/leader)
  raft_state state;
  void run_follower();
  void run_candidate();
  void run_leader();
public:
  raft_provider(tl::engine& e,uint16_t provider_id=1);
  ~raft_provider();
  void run();
};

#endif