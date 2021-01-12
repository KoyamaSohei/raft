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

raft_provider::raft_provider(tl::engine& e,uint16_t provider_id)
  : tl::provider<raft_provider>(e, provider_id),
    state(raft_state::follower)
{
  get_engine().push_finalize_callback(this,[p=this]() {delete p;});
}

raft_provider::~raft_provider() {
  get_engine().pop_finalize_callback(this);
}

void raft_provider::run_follower() {

}

void raft_provider::run_candidate() {

}

void raft_provider::run_leader() {

}

void raft_provider::run() {
  while(1) {
    switch (state) {
    case raft_state::follower:
      run_follower();
      break;
    case raft_state::candidate:
      run_candidate();
      break;
    case raft_state::leader:
      run_leader();
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

  tl::engine my_engine("tcp", THALLIUM_SERVER_MODE);
  std::cout << "Server running at address " << my_engine.self() << std::endl;
  raft_provider provider(my_engine);
  
  ABT_xstream_create(ABT_SCHED_NULL,&tickstream);

  while(1) {
    sleep(INTERVAL);
    ABT_thread_create_on_xstream(tickstream,tick_loop,&provider,ABT_THREAD_ATTR_NULL,&tickthread);
  }

  my_engine.wait_for_finalize();
  return 0;
}