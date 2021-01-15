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
  raft_state s = _state;
  return s;
}

void raft_provider::set_state(raft_state new_state) {
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
}

int raft_provider::get_current_term() {
  int t = _current_term;
  return t;
}

int raft_provider::get_commit_index() {
  int c = _commit_index;
  return c;
}

void raft_provider::set_commit_index(int index) {
  _commit_index = index;
}

void raft_provider::update_timeout_limit() {
  int span = 0;
  switch(_state) {
  case raft_state::follower:
    span = 2;
    break;
  case raft_state::candidate:
    span = 2 + rand() % 10;
    break;
  default:
    abort();
  }
  timeout_limit = system_clock::now() + std::chrono::seconds(span*INTERVAL);
}

void raft_provider::set_force_current_term(int term) {
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
      update_timeout_limit();
      break;
  }
}

append_entries_response raft_provider::append_entries_rpc(append_entries_request &req) {
  mu.lock();
  int current_term = get_current_term();
  if(req.get_term() < current_term) {
    mu.unlock();
    return append_entries_response(current_term,false);
  }

  if(req.get_term() > current_term) {
    set_force_current_term(req.get_term());
    mu.unlock();
    return append_entries_response(current_term,false);
  }

  switch(get_state()) {
    case raft_state::ready:
      mu.unlock();
      return append_entries_response(current_term,false);
    case raft_state::candidate:
      become_follower();
      mu.unlock();
      return append_entries_response(current_term,false);
    case raft_state::leader:
      printf("there are 2 leader in same term\n");
      mu.unlock();
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
    mu.unlock();
    return append_entries_response(current_term,false);
  }
  std::vector<raft_entry> entries(req.get_entries());

  for(raft_entry ent:entries) {
    logger.save_log(ent.get_index(),req.get_term(),ent.get_key(),ent.get_value());
  }

  if(req.get_leader_commit() > get_commit_index()) {
    int next_index = req.get_leader_commit();
    if(!entries.empty()) {
      next_index = std::min(next_index,entries.back().get_index());
    }
    set_commit_index(next_index);
  }
  mu.unlock();
  return append_entries_response(current_term,true);
}

request_vote_response raft_provider::request_vote_rpc(request_vote_request &req) {
  mu.lock();
  int current_term = get_current_term();
  std::string candidate_id = req.get_candidate_id();
  int request_term = req.get_term();

  printf("request_vote_rpc from %s in term %d\n",candidate_id.c_str(),request_term);

  if(request_term  < current_term) {
    mu.unlock();
    return request_vote_response(current_term,false);
  }

  int last_log_index,last_log_term;
  logger.get_last_log(last_log_index,last_log_term);

  if(request_term  > current_term) {
    set_force_current_term(request_term);
    
    if(last_log_index != req.get_last_log_index()) {
      mu.unlock();
      return request_vote_response(current_term,false);
    }
    logger.save_voted_for(candidate_id);
    _voted_for = candidate_id;

    update_timeout_limit();
    mu.unlock();
    return request_vote_response(current_term,true);
  }
  
  if(!_voted_for.empty() && _voted_for != candidate_id) {
    mu.unlock();
    return request_vote_response(current_term,false);
  }

  if(last_log_index != req.get_last_log_index()) {
    mu.unlock();
    return request_vote_response(current_term,false);
  }

  if(last_log_term != req.get_last_log_term()) {
    mu.unlock();
    return request_vote_response(current_term,false);
  }

  if(_voted_for == candidate_id) {
    mu.unlock();
    return request_vote_response(current_term,false);
  }

  assert(_voted_for.empty());

  logger.save_voted_for(candidate_id);
  _voted_for = candidate_id;

  update_timeout_limit();
  mu.unlock();
  return request_vote_response(current_term,true);
}

int raft_provider::client_put_rpc(std::string key,std::string value) {
  if(get_state()!=raft_state::leader) {
    return RAFT_NODE_IS_NOT_LEADER;
  }
  int term = get_current_term();
  logger.append_log(term,key,value);
  return RAFT_NOT_IMPLEMENTED;
}

client_get_response raft_provider::client_get_rpc(std::string key) {
  if(get_state()!=raft_state::leader) {
    return client_get_response(RAFT_NODE_IS_NOT_LEADER,"");
  }
  return client_get_response(RAFT_SUCCESS,kvs.get(key));
}

int raft_provider::echo_state_rpc() {
  return (int)get_state();
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

  int last_log_index,last_log_term;
  logger.get_last_log(last_log_index,last_log_term);
  int current_term = get_current_term();

  request_vote_request req(current_term,id,last_log_index,last_log_term);
  int vote = 1;


  for(tl::provider_handle node: nodes) {
    mu.unlock();
    printf("request_vote to %s\n",std::string(node).c_str());
    request_vote_response resp = m_request_vote_rpc.on(node)(req);
    mu.lock();
    if(get_state() == raft_state::follower) {
      return;
    }
    assert(get_state() == raft_state::candidate);
    if(resp.get_term()>current_term) {
      become_follower();
      return;
    }
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
  if(system_clock::now() > timeout_limit) {
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
  for(tl::provider_handle node:nodes) {
    next_index[&node]=last_index+1;
  }
  match_index.clear();
  // matchIndex initialized to 0
  for(tl::provider_handle node:nodes) {
    match_index[&node]=0;
  }
  set_state(raft_state::leader);
}

void raft_provider::run_leader() {
  int term = get_current_term();
  int commit_index = get_commit_index();
  int last_index,last_term;
  logger.get_last_log(last_index,last_term);

  for(tl::provider_handle node:nodes) {
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
    mu.unlock();
    append_entries_response resp = m_append_entries_rpc.on(node)(req);
    mu.lock();
    if(get_state() == raft_state::follower) {
      return;
    }
    assert(get_state() == raft_state::candidate);
    if(resp.get_term()>get_current_term()) {
      become_follower();
      return;
    }
    if(resp.is_success()) {
      match_index[&node]=last_index;
      next_index[&node]=last_index+1;
    } else {
      next_index[&node]--;
    }
  }
  // check if leader can commit `commit_index+1`
  if(last_index > commit_index) {
    int count = 1;
    for(tl::provider_handle node:nodes) {
      if(match_index[&node]>=commit_index+1) {
        count++;
      }
    }
    if(count*2 > num_nodes && logger.get_term(commit_index+1)==term) {
      set_commit_index(commit_index+1);
    }
  }
}

void raft_provider::run() {
  mu.lock();
  int last_applied = kvs.get_last_applied();
  if(last_applied<get_commit_index()) {
    int t;
    std::string k,v;
    logger.get_log(last_applied+1,t,k,v);
    kvs.apply(last_applied+1,k,v);
  }
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
  printf("current_term: %d, voted_for: %s\n",get_current_term(),_voted_for.c_str());
  mu.unlock();
}

void raft_provider::append_node(std::string addr) {
  assert(get_state()==raft_state::ready);
  tl::endpoint p = get_engine().lookup(addr);
  nodes.push_back(tl::provider_handle(p,RAFT_PROVIDER_ID));
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