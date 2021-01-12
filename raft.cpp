#include <iostream>
#include <thallium.hpp>
#include <thread>
#include <pthread.h>
#include <chrono>
#include <abt.h>
#include <unistd.h>
#include "raft.hpp"

#define INTERVAL 1
#define TIMEOUT  3

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

}

void RaftProvider::runCandidate() {

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

void tick_loop(void *provider) {
  printf("tick!\n");
}

void setup_segset(sigset_t *ss) {
  sigemptyset(ss);
  sigaddset(ss, SIGINT);
  sigaddset(ss, SIGTERM);
  pthread_sigmask(SIG_BLOCK, ss, NULL);
}

int main(int argc, char** argv) {

  ABT_xstream sigstream,tickstream;
  ABT_thread sigthread,tickthread;
  static sigset_t ss;
  
  setup_segset(&ss);

  ABT_init(argc,argv);
  
  ABT_xstream_create(ABT_SCHED_NULL,&sigstream);
  ABT_thread_create_on_xstream(sigstream,signal_handler,&ss,ABT_THREAD_ATTR_NULL,&sigthread);

  tl::engine myEngine("tcp", THALLIUM_SERVER_MODE);
  std::cout << "Server running at address " << myEngine.self() << std::endl;
  RaftProvider provider(myEngine);
  
  ABT_xstream_create(ABT_SCHED_NULL,&tickstream);

  while(1) {
    sleep(INTERVAL);
    ABT_thread_create_on_xstream(tickstream,tick_loop,&provider,ABT_THREAD_ATTR_NULL,&tickthread);
  }

  myEngine.wait_for_finalize();
  return 0;
}