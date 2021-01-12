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
    state(raft_state::follower),
    last_entry_recerived(clock::now()),
    m_append_entries_rpc(define("append_entries",&raft_provider::append_entries_rpc))
{
  get_engine().push_finalize_callback(this,[p=this]() {delete p;});
}

raft_provider::~raft_provider() {
  get_engine().pop_finalize_callback(this);
}

void raft_provider::append_entries_rpc(tl::request req) {
  last_entry_recerived = clock::now();
}

void raft_provider::run_follower() {
  auto duration = clock::now() - last_entry_recerived;
  if(duration > std::chrono::seconds(TIMEOUT)) {
    become_candidate();
  }
}

void raft_provider::become_candidate() {
  printf("become candidate\n");
  state = raft_state::candidate;
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

  ABT_xstream sig_stream,tick_stream;
  ABT_thread sig_thread,tick_thread;
  static sigset_t ss;
  
  setup_segset(&ss);

  ABT_init(argc,argv);
  
  ABT_xstream_create(ABT_SCHED_NULL,&sig_stream);
  ABT_thread_create_on_xstream(sig_stream,signal_handler,&ss,ABT_THREAD_ATTR_NULL,&sig_thread);

  tl::engine my_engine("tcp", THALLIUM_SERVER_MODE);
  std::cout << "Server running at address " << my_engine.self() << std::endl;
  raft_provider provider(my_engine);
  
  ABT_xstream_create(ABT_SCHED_NULL,&tick_stream);

  while(1) {
    sleep(INTERVAL);
    ABT_thread_create_on_xstream(tick_stream,tick_loop,&provider,ABT_THREAD_ATTR_NULL,&tick_thread);
  }

  my_engine.wait_for_finalize();
  return 0;
}