#include <iostream>
#include <thallium.hpp>
#include <thread>
#include <pthread.h>
#include <chrono>
#include <abt.h>
#include <unistd.h>
#include <cassert>
#include "raft.hpp"
#include "types.hpp"

#define INTERVAL 1
#define TIMEOUT  3

raft_provider::raft_provider(tl::engine& e,uint16_t provider_id)
  : tl::provider<raft_provider>(e, provider_id),
    id(get_engine().self()),
    _state(raft_state::follower),
    last_entry_recerived(system_clock::now()),
    num_nodes(1),
    _current_term(0),
    m_append_entries_rpc(define("append_entries",&raft_provider::append_entries_rpc)),
    m_request_vote_rpc(define("request_vote",&raft_provider::request_vote_rpc))
{
  get_engine().push_finalize_callback(this,[p=this]() {delete p;});
}

raft_provider::~raft_provider() {
  get_engine().pop_finalize_callback(this);
}

raft_state raft_provider::get_state() {
  mu.lock();
  raft_state s = _state;
  mu.unlock();
  return s;
}

void raft_provider::set_state(raft_state new_state) {
  mu.lock();
  _state = new_state;
  mu.unlock();
}

int raft_provider::get_current_term() {
  mu.lock();
  int t = _current_term;
  mu.unlock();
  return t;
}

void raft_provider::increment_current_term() {
  mu.lock();
  _current_term++;
  mu.unlock();
}

append_entries_response raft_provider::append_entries_rpc(append_entries_request &req) {
  last_entry_recerived = system_clock::now();
  return append_entries_response(0,false);
}

request_vote_response raft_provider::request_vote_rpc(request_vote_request &req) {
  int current_term = get_current_term();
  if(req.get_term() < current_term) {
    return request_vote_response(current_term,false);
  }
  return request_vote_response(current_term,true);
}

void raft_provider::run_follower() {
  auto duration = system_clock::now() - last_entry_recerived;
  if(duration > std::chrono::seconds(TIMEOUT)) {
    become_candidate();
    return;
  }
}

void raft_provider::become_candidate() {
  printf("become candidate\n");
  set_state(raft_state::candidate);
  request_vote_request req(get_current_term(),id);
  int vote = 1;
  for(tl::endpoint node: nodes) {
    request_vote_response resp = m_request_vote_rpc.on(node)(req);
    if(resp.is_vote_granted()) {
      vote++;
    }
  }
  if(vote * 2 > num_nodes) {
    become_leader();
    return;
  }
}

void raft_provider::run_candidate() {

}

void raft_provider::become_leader() {
  printf("become leader\n");
  set_state(raft_state::leader);
}

void raft_provider::run_leader() {

}

void raft_provider::run() {
  while(1) {
    switch (get_state()) {
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

void raft_provider::append_node(std::string addr) {
  nodes.push_back(get_engine().lookup(addr));
  num_nodes++;
  assert(num_nodes==(int)nodes.size()+1);
}

void signal_handler(void *arg) {
  int num;
  sigwait((sigset_t *)arg,&num);
  std::cout << "Signal received " << num << std::endl;
  exit(1);
}

void tick_loop(void *provider) {
  printf("tick!\n");
  ((raft_provider *)provider)->run();
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

  if(argc > 1) {
    for(int i=1;i < argc;i++) {
      provider.append_node(argv[i]);
    }
  }
  
  ABT_xstream_create(ABT_SCHED_NULL,&tick_stream);

  while(1) {
    sleep(INTERVAL);
    ABT_thread_create_on_xstream(tick_stream,tick_loop,&provider,ABT_THREAD_ATTR_NULL,&tick_thread);
  }

  my_engine.wait_for_finalize();
  return 0;
}