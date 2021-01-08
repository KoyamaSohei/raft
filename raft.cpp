#include <iostream>
#include <thallium.hpp>
#include <thread>
#include <pthread.h>
#include <chrono>

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
  tl::mutex mu;
  tl::condition_variable cond;
  const int TIMEOUT = 3;
public:
  RaftProvider(tl::engine& e,uint16_t provider_id=1)
  : tl::provider<RaftProvider>(e, provider_id),
    state(State::Follower)
  {
    get_engine().push_finalize_callback(this,[p=this]() {delete p;});
  }
  ~RaftProvider() {
    get_engine().pop_finalize_callback(this);
  }
  void runFollower() {
    std::cout << "become follower" << std::endl;
    std::unique_lock<tl::mutex> lock(mu);
    while(1) {
      timespec deadline;
      timespec_get(&deadline,TIME_UTC);
      deadline.tv_sec += TIMEOUT;
      bool acquired = cond.wait_until(lock,&deadline);
      if(!acquired) { // timeout
        state = State::Candidate;
        break;
      }
    }
  }
  void runCandidate() {
    std::cout << "become candidate" << std::endl;
    while(1) {
      
    }
  }
  void runLeader() {

  }
  void run() {
    while(1) {
      switch (state) {
      case State::Follower:
        runFollower();
        break;
      case State::Candidate:
        runCandidate();
        break;
      case State::Leader:
        runLeader();
        break;
      }
    }
  }
};

int main(int argc, char** argv) {
  static sigset_t ss;
  sigemptyset(&ss);
  sigaddset(&ss, SIGINT);
  sigaddset(&ss, SIGTERM);
  pthread_sigmask(SIG_BLOCK, &ss, NULL);

  tl::engine myEngine("tcp", THALLIUM_SERVER_MODE);
  std::cout << "Server running at address " << myEngine.self() << std::endl;
  RaftProvider provider(myEngine);

  std::thread sig([&]{
    int num;
    sigwait(&ss,&num);
    std::cout << "Signal received " << num << std::endl;
    exit(1);
  });
  provider.run();
  myEngine.wait_for_finalize();
  return 0;
}