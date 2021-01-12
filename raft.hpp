#ifndef RAFT_HPP
#define RAFT_HPP

#include <thallium.hpp>

namespace tl = thallium;

enum class State {
  Follower,
  Candidate,
  Leader,
};

class RaftProvider : public tl::provider<RaftProvider> {
private:
  // 現在の状態(Follower/Candidate/Leader)
  State state;
  void runFollower();
  void runCandidate();
  void runLeader();
public:
  RaftProvider(tl::engine& e,uint16_t provider_id=1);
  ~RaftProvider();
  void run();
};

#endif