#include <cassert>
#include "provider.hpp"

raft_provider::raft_provider(tl::engine& e,uint16_t provider_id)
  : tl::provider<raft_provider>(e, provider_id),
    id(get_engine().self()),
    _state(raft_state::ready),
    num_nodes(1),
    _current_term(0),
    m_append_entries_rpc(define("append_entries",&raft_provider::append_entries_rpc)),
    m_request_vote_rpc(define("request_vote",&raft_provider::request_vote_rpc))
{
  become_follower();
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
  switch(new_state) {
  case raft_state::follower:
    assert(_state==raft_state::ready     ||
           _state==raft_state::candidate ||
           _state==raft_state::leader);
    break;
  case raft_state::candidate:
    assert(_state==raft_state::follower ||
           _state==raft_state::candidate);
    _current_term++;
    _voted_for=id;
    break;
  case raft_state::leader:
    assert(_state==raft_state::candidate);
    break;
  default:
    abort();
  }
  _state = new_state;
  mu.unlock();
}

int raft_provider::get_current_term() {
  mu.lock();
  int t = _current_term;
  mu.unlock();
  return t;
}

void raft_provider::update_timeout_limit() {
  timeout_limit = system_clock::now() + std::chrono::seconds(TIMEOUT + rand() % TIMEOUT);
}

append_entries_response raft_provider::append_entries_rpc(append_entries_request &req) {
  update_timeout_limit();
  return append_entries_response(0,false);
}

request_vote_response raft_provider::request_vote_rpc(request_vote_request &req) {
  int current_term = get_current_term();
  if(req.get_term() < current_term) {
    return request_vote_response(current_term,false);
  }
  mu.lock();
  if(_voted_for.empty() ||
    _voted_for==req.get_candidate_id()) {
    _voted_for = req.get_candidate_id();
    mu.unlock();
    return request_vote_response(current_term,true);
  }
  mu.unlock();
  return request_vote_response(current_term,false);
}

void raft_provider::become_follower() {
  printf("become follower\n");
  set_state(raft_state::follower);
  update_timeout_limit();
}

void raft_provider::run_follower() {
  if(system_clock::now() > timeout_limit) {
    update_timeout_limit();
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
    if(resp.get_term()>get_current_term()) {
      become_follower();
      return;
    }
    if(resp.is_vote_granted()) {
      vote++;
    }
  }
  if(vote * 2 > num_nodes && get_state() == raft_state::candidate) {
    become_leader();
    return;
  }
}

void raft_provider::run_candidate() {
  if(system_clock::now() > timeout_limit) {
    update_timeout_limit();
    become_candidate();
  }
}

void raft_provider::become_leader() {
  printf("become leader\n");
  set_state(raft_state::leader);
}

void raft_provider::run_leader() {

}

void raft_provider::run() {
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

void raft_provider::append_node(std::string addr) {
  nodes.push_back(get_engine().lookup(addr));
  num_nodes++;
  assert(num_nodes==(int)nodes.size()+1);
}