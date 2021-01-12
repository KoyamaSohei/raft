#include <iostream>
#include <thallium.hpp>
#include <thread>
#include <pthread.h>
#include <chrono>
#include <abt.h>
#include "raft.hpp"

namespace tl = thallium;

RaftProvider::RaftProvider(tl::engine& e,uint16_t provider_id)
  : tl::provider<RaftProvider>(e, provider_id),
    state(State::Follower)
{
  get_engine().push_finalize_callback(this,[p=this]() {delete p;});
}

RaftProvider::~RaftProvider() {
  get_engine().pop_finalize_callback(this);
}

void RaftProvider::runFollower() {
  std::cout << "[follower] become" << std::endl;
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

void RaftProvider::runCandidate() {
  std::cout << "[candidate] become" << std::endl;
  while(1) {
    
  }
}

void RaftProvider::runLeader() {

}

void RaftProvider::run() {
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

void signal_handler(void *arg) {
  int num;
  sigwait((sigset_t *)arg,&num);
  std::cout << "Signal received " << num << std::endl;
  exit(1);
}

void setup_segset(sigset_t *ss) {
  sigemptyset(ss);
  sigaddset(ss, SIGINT);
  sigaddset(ss, SIGTERM);
  pthread_sigmask(SIG_BLOCK, ss, NULL);
}

int main(int argc, char** argv) {

  ABT_xstream sigstream;
  ABT_thread thread;
  static sigset_t ss;
  
  setup_segset(&ss);

  ABT_init(argc,argv);
  
  ABT_xstream_create(ABT_SCHED_NULL,&sigstream);
  ABT_thread_create_on_xstream(sigstream,signal_handler,&ss,ABT_THREAD_ATTR_NULL,&thread);

  tl::engine myEngine("tcp", THALLIUM_SERVER_MODE);
  std::cout << "Server running at address " << myEngine.self() << std::endl;
  RaftProvider provider(myEngine);

  myEngine.wait_for_finalize();
  return 0;
}