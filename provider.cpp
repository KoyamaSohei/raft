#include <cassert>
#include <unistd.h>
#include "provider.hpp"

raft_provider::raft_provider(tl::engine& e,uint16_t provider_id)
  : tl::provider<raft_provider>(e, provider_id),
    id(get_engine().self()),
    _state(raft_state::ready),
    num_nodes(1),
    _current_term(0),
    logger(id),
    _commit_index(0),
    m_append_entries_rpc(define("append_entries",&raft_provider::append_entries_rpc)),
    m_request_vote_rpc(define("request_vote",&raft_provider::request_vote_rpc))
{
  define(CLIENT_PUT_RPC_NAME,&raft_provider::client_put_rpc);
  define(CLIENT_GET_RPC_NAME,&raft_provider::client_get_rpc);
  define(ECHO_STATE_RPC_NAME,&raft_provider::echo_state_rpc);
  // bootstrap state from (already exist) log
  logger.bootstrap_state_from_log(_current_term,_voted_for);
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
    logger.save_current_term(_current_term+1);
    _current_term++;
    logger.save_voted_for(id);
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

int raft_provider::get_commit_index() {
  mu.lock();
  int c = _commit_index;
  mu.unlock();
  return c;
}

void raft_provider::update_timeout_limit() {
  timeout_limit = system_clock::now() + std::chrono::seconds(TIMEOUT + rand() % TIMEOUT);
}

void raft_provider::set_force_current_term(int term) {
  mu.lock();
  assert(_current_term<term);
  logger.save_current_term(term);
  _current_term = term;
  logger.save_voted_for("");
  _voted_for.clear();
  switch(_state) {
    case raft_state::ready:
    case raft_state::follower:
      break;
    case raft_state::candidate:
    case raft_state::leader:
      _state = raft_state::follower;
      break;
  }
  mu.unlock();
}

append_entries_response raft_provider::append_entries_rpc(append_entries_request &req) {
  int current_term = get_current_term();
  if(req.get_term() < current_term) {
    return append_entries_response(current_term,false);
  }

  if(req.get_term() > current_term) {
    set_force_current_term(req.get_term());
    return append_entries_response(current_term,false);
  }

  switch(get_state()) {
    case raft_state::ready:
      return append_entries_response(current_term,false);
    case raft_state::candidate:
      become_follower();
      return append_entries_response(current_term,false);
    case raft_state::leader:
      printf("there are 2 leader in same term\n");
      abort();
      return append_entries_response(current_term,false);
    case raft_state::follower:
      // run below
      break;
  }

  assert(get_state()==raft_state::follower);
  update_timeout_limit();

  bool is_match = logger.match_log(req.get_prev_index(),req.get_prev_term());
  if(!is_match) {
    return append_entries_response(current_term,false);
  }
  
  return append_entries_response(current_term,true);
}

request_vote_response raft_provider::request_vote_rpc(request_vote_request &req) {
  int current_term = get_current_term();
  std::string candidate_id = req.get_candidate_id();
  int request_term = req.get_term();
  if(request_term  < current_term) {
    return request_vote_response(current_term,false);
  }
  if(request_term  > current_term) {
    set_force_current_term(request_term );
    mu.lock();
    logger.save_voted_for(req.get_candidate_id());
    _voted_for = req.get_candidate_id();
    mu.unlock();
    return request_vote_response(request_term ,true);
  }

  mu.lock();
  if(_voted_for.empty() ||
    _voted_for==req.get_candidate_id()) {
    logger.save_voted_for(req.get_candidate_id());
    _voted_for = req.get_candidate_id();
    mu.unlock();
    return request_vote_response(current_term,true);
  }
  mu.unlock();
  return request_vote_response(current_term,false);
}

int raft_provider::client_put_rpc(std::string key,std::string value) {
  if(get_state()!=raft_state::leader) {
    return RAFT_NODE_IS_NOT_LEADER;
  }
  int term = get_current_term();
  mu.lock();
  logger.append_log(term,key,value);
  mu.unlock();
  return RAFT_NOT_IMPLEMENTED;
}

client_get_response raft_provider::client_get_rpc(std::string key) {
  if(get_state()!=raft_state::leader) {
    return client_get_response(RAFT_NODE_IS_NOT_LEADER,"");
  }
  return client_get_response();
}

int raft_provider::echo_state_rpc() {
  mu.lock();
  int r = (int)get_state();
  mu.unlock();
  return r;
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
  printf("become candidate, and starting election...\n");
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
  int last_index;
  int last_term;
  logger.get_last_log(last_index,last_term);
  next_index.clear();
  // next_index initialized to leader last log index + 1
  for(tl::endpoint node:nodes) {
    next_index[&node]=last_index+1;
  }
  match_index.clear();
  // matchIndex initialized to 0
  for(tl::endpoint node:nodes) {
    match_index[&node]=0;
  }
  set_state(raft_state::leader);
}

void raft_provider::run_leader() {
  int term = get_current_term();
  int commit_index = get_commit_index();
  int last_index,last_term;
  logger.get_last_log(last_index,last_term);

  for(tl::endpoint node:nodes) {
    int prev_index = next_index[&node]-1;
    int prev_term = logger.get_term(prev_index);
    std::vector<raft_entry> entries;
    for(int idx=prev_index+1;idx <= last_index;idx++) {
      int t;
      std::string k,v;
      logger.get_log(idx,t,k,v);
      assert(t==term);
      entries.emplace_back(idx,k,v);
    }
    append_entries_request req(term,prev_index,prev_term,entries,commit_index);
    append_entries_response resp = m_append_entries_rpc.on(node)(req);
    if(resp.get_term()>term) {
      become_follower();
      return;
    }
    if(resp.is_success()) {
      match_index[&node]=last_index;
      next_index[&node]=last_index+1;
    }
  }
}

void raft_provider::run() {
  switch (get_state()) {
  case raft_state::ready:
    break;
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
  assert(get_state()==raft_state::ready);
  nodes.push_back(get_engine().lookup(addr));
  num_nodes++;
  assert(num_nodes==(int)nodes.size()+1);
}

void raft_provider::start(std::vector<std::string> &addrs) {
  // append_node(addr)時に名前解決するために、他のサーバーが起動するのを待つ
  sleep(INTERVAL);
  for(std::string addr:addrs) {
    append_node(addr);
  }
  become_follower();
}