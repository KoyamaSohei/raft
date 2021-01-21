#include "provider.hpp"

#include <unistd.h>

#include <cassert>

raft_provider::raft_provider(tl::engine &e, uint16_t provider_id)
  : tl::provider<raft_provider>(e, provider_id)
  , id(get_engine().self())
  , _state(raft_state::ready)
  , num_nodes(1)
  , _current_term(0)
  , logger(id)
  , _commit_index(0)
  , m_append_entries_rpc(
      define("append_entries", &raft_provider::append_entries_rpc))
  , m_request_vote_rpc(define("request_vote", &raft_provider::request_vote_rpc))
  , m_client_put_rpc(
      define(CLIENT_PUT_RPC_NAME, &raft_provider::client_put_rpc))
  , m_client_get_rpc(
      define(CLIENT_GET_RPC_NAME, &raft_provider::client_get_rpc)) {
  define(ECHO_STATE_RPC_NAME, &raft_provider::echo_state_rpc);
  // bootstrap state from (already exist) log
  logger.bootstrap_state_from_log(_current_term, _voted_for);
  get_engine().push_finalize_callback(this, [p = this]() { delete p; });
  // Block RPC until _state != ready
  mu.lock();
}

raft_provider::~raft_provider() {
  get_engine().pop_finalize_callback(this);
}

raft_state raft_provider::get_state() {
  return _state;
}

void raft_provider::set_state(raft_state new_state) {
  switch (new_state) {
    case raft_state::follower:
      assert(_state == raft_state::ready || _state == raft_state::candidate ||
             _state == raft_state::leader);
      break;
    case raft_state::candidate:
      assert(_state == raft_state::follower || _state == raft_state::candidate);
      logger.save_current_term(_current_term + 1);
      _current_term++;
      logger.save_voted_for(id);
      _voted_for = id;
      break;
    case raft_state::leader:
      assert(_state == raft_state::candidate);
      break;
    default:
      abort();
  }
  _state = new_state;
  leader_id = tl::provider_handle();
}

int raft_provider::get_current_term() {
  assert(0 <= _current_term);
  return _current_term;
}

int raft_provider::get_commit_index() {
  assert(0 <= _commit_index);
  return _commit_index;
}

void raft_provider::set_commit_index(int index) {
  assert(_commit_index < index);
  _commit_index = index;
}

void raft_provider::update_timeout_limit() {
  int span = 0;
  switch (_state) {
    case raft_state::follower:
      span = 2;
      break;
    case raft_state::candidate:
      span = 2 + rand() % 10;
      break;
    default:
      abort();
  }
  timeout_limit =
    system_clock::now() + std::chrono::microseconds(span * INTERVAL);
}

void raft_provider::set_force_current_term(int term) {
  assert(_current_term < term);
  logger.save_current_term(term);
  _current_term = term;
  logger.save_voted_for("");
  _voted_for.clear();

  switch (_state) {
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

void raft_provider::append_entries_rpc(const tl::request &r, int req_term,
                                       int req_prev_index, int req_prev_term,
                                       std::vector<raft_entry> req_entries,
                                       int req_leader_commit,
                                       std::string req_leader_id) {
  mu.lock();
  int current_term = get_current_term();
  if (req_term < current_term) {
    mu.unlock();
    try {
      r.respond(append_entries_response(current_term, false));
    } catch (tl::exception &e) {
      printf("error respond to %s\n", std::string(r.get_endpoint()).c_str());
    }
    return;
  }

  leader_id =
    tl::provider_handle(get_engine().lookup(req_leader_id), RAFT_PROVIDER_ID);

  if (req_term > current_term) {
    set_force_current_term(req_term);
    assert(get_state() == raft_state::follower);
  }

  switch (get_state()) {
    case raft_state::ready:
      mu.unlock();
      abort();
    case raft_state::candidate:
      become_follower();
      break;
    case raft_state::leader:
      printf("there are 2 leader in same term\n");
      mu.unlock();
      abort();
      return;
    case raft_state::follower:
      // run below
      break;
  }

  assert(get_state() == raft_state::follower);
  update_timeout_limit();

  bool is_match = logger.match_log(req_prev_index, req_prev_term);
  if (!is_match) {
    mu.unlock();
    try {
      r.respond(append_entries_response(current_term, false));
    } catch (tl::exception &e) {
      printf("error respond to %s\n", std::string(r.get_endpoint()).c_str());
    }
    return;
  }

  for (raft_entry ent : req_entries) {
    printf("entry received, idx: %d, term: %d, key: %s, value: %s\n",
           ent.get_index(), req_term, ent.get_key().c_str(),
           ent.get_value().c_str());
    logger.save_log(ent.get_index(), req_term, ent.get_key(), ent.get_value());
  }

  if (req_leader_commit > get_commit_index()) {
    int next_index = req_leader_commit;
    if (!req_entries.empty()) {
      next_index = std::min(next_index, req_entries.back().get_index());
    }
    set_commit_index(next_index);
  }
  mu.unlock();
  try {
    r.respond(append_entries_response(current_term, true));
  } catch (tl::exception &e) {
    printf("error respond to %s\n", std::string(r.get_endpoint()).c_str());
  }
  return;
}

void raft_provider::request_vote_rpc(const tl::request &r, int req_term,
                                     std::string req_candidate_id,
                                     int req_last_log_index,
                                     int req_last_log_term) {
  mu.lock();
  int current_term = get_current_term();

  printf("request_vote_rpc from %s in term %d\n", req_candidate_id.c_str(),
         req_term);

  int last_log_index, last_log_term;
  logger.get_last_log(last_log_index, last_log_term);

  bool granted = [&]() -> bool {
    if (req_term < current_term) return false;
    if (req_term > current_term) { set_force_current_term(req_term); }
    if (!_voted_for.empty()) return false;
    if (req_last_log_term < last_log_term) return false;
    if (req_last_log_term > last_log_term) return true;
    if (req_last_log_index < last_log_index) return false;
    return true;
  }();

  current_term = get_current_term();

  if (granted) {
    logger.save_voted_for(req_candidate_id);
    _voted_for = req_candidate_id;
  }

  mu.unlock();
  try {
    r.respond(request_vote_response(current_term, granted));
  } catch (tl::exception &e) {
    printf("error respond to %s\n", std::string(r.get_endpoint()).c_str());
  }
  return;
}

void raft_provider::client_put_rpc(const tl::request &r, std::string key,
                                   std::string value) {
  mu.lock();
  if (get_state() != raft_state::leader) {
    mu.unlock();
    if (leader_id.is_null()) {
      try {
        r.respond(client_put_response(RAFT_LEADER_NOT_FOUND));
      } catch (tl::exception &e) {}
      return;
    }
    try {
      client_put_response resp = m_client_put_rpc.on(leader_id)(key, value);
      r.respond(resp);
    } catch (tl::exception &e) {
      try {
        r.respond(client_put_response(RAFT_LEADER_NOT_FOUND));
      } catch (tl::exception &e) {}
    }

    return;
  }
  int index = logger.append_log(get_current_term(), key, value);
  mu.unlock();
  try {
    r.respond(client_put_response(RAFT_SUCCESS, index));
  } catch (tl::exception &e) {}
}

void raft_provider::client_get_rpc(const tl::request &r, std::string key) {
  mu.lock();
  if (get_state() != raft_state::leader) {
    if (leader_id.is_null()) {
      mu.unlock();
      try {
        r.respond(client_get_response(RAFT_LEADER_NOT_FOUND, ""));
      } catch (tl::exception &e) {}
      return;
    }
    mu.unlock();
    try {
      client_get_response resp = m_client_get_rpc.on(leader_id)(key);
      r.respond(resp);
    } catch (tl::exception &e) {
      try {
        r.respond(RAFT_LEADER_NOT_FOUND);
      } catch (tl::exception &e) {}
    }
    return;
  }
  mu.unlock();
  try {
    r.respond(client_get_response(RAFT_SUCCESS, kvs.get(key)));
  } catch (tl::exception &e) {}
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
  if (system_clock::now() > timeout_limit) {
    update_timeout_limit();
    become_candidate();
    return;
  }
}

void raft_provider::become_candidate() {
  printf("become candidate, and starting election...\n");
  set_state(raft_state::candidate);

  int last_log_index, last_log_term;
  logger.get_last_log(last_log_index, last_log_term);
  int current_term = get_current_term();

  int vote = 1;

  for (std::string node : nodes) {
    mu.unlock();
    printf("request_vote to %s\n", node.c_str());
    request_vote_response resp;
    try {
      resp = m_request_vote_rpc.on(node_to_handle[node])(
        current_term, id, last_log_index, last_log_term);
    } catch (const tl::exception &e) {
      printf("error occured at node %s\n", node.c_str());
      mu.lock();
      continue;
    }
    mu.lock();
    if (get_state() == raft_state::follower) { return; }
    assert(get_state() == raft_state::candidate);
    if (resp.get_term() > current_term) {
      become_follower();
      return;
    }
    if (resp.is_vote_granted()) { vote++; }
  }
  if (vote * 2 > num_nodes) {
    become_leader();
    return;
  }
}

void raft_provider::run_candidate() {
  if (system_clock::now() > timeout_limit) { become_candidate(); }
}

void raft_provider::become_leader() {
  printf("become leader\n");
  int last_index;
  int last_term;
  logger.get_last_log(last_index, last_term);
  next_index.clear();
  // next_index initialized to leader last log index + 1
  for (std::string node : nodes) { next_index[node] = last_index + 1; }
  match_index.clear();
  // match_index initialized to 0
  for (std::string node : nodes) { match_index[node] = 0; }
  // progress_state initialized to probe
  for (std::string node : nodes) {
    progress_state[node] = raft_progress_state::probe;
  }
  set_state(raft_state::leader);
}

void raft_provider::run_leader() {
  int term = get_current_term();
  int commit_index = get_commit_index();
  int last_log_index, _;
  logger.get_last_log(last_log_index, _);

  for (std::string node : nodes) {
    int prev_index = next_index[node] - 1;
    assert(0 <= prev_index);
    int prev_term = logger.get_term(prev_index);
    std::vector<raft_entry> entries;
    int last_index =
      std::min(last_log_index, next_index[node] + MAX_ENTRIES_NUM);
    for (int idx = next_index[node]; idx <= last_index; idx++) {
      int t;
      std::string k, v;
      logger.get_log(idx, t, k, v);
      assert(t == term);
      entries.emplace_back(idx, k, v);
    }

    mu.unlock();
    append_entries_response resp;
    try {
      resp = m_append_entries_rpc.on(node_to_handle[node])(
        term, prev_index, prev_term, entries, commit_index, id);
    } catch (const tl::exception &e) {
      printf("error occured at node %s\n", node.c_str());
      mu.lock();
      continue;
    }
    mu.lock();
    if (get_state() == raft_state::follower) { return; }
    assert(get_state() == raft_state::leader);

    if (resp.get_term() > get_current_term()) {
      become_follower();
      return;
    }

    if (resp.is_success()) {
      match_index[node] = last_index;
      next_index[node] = last_index + 1;
    } else {
      next_index[node]--;
      assert(next_index[node] > 0);
    }

    printf("node %s match: %d, next: %d\n", node.c_str(), match_index[node],
           next_index[node]);
  }
  // check if leader can commit N
  // N := sorted_match_index[num_nodes/2]
  //
  // explain why N can be commited
  // match_index[]        = {3 1 4 2 5} (leader's match_index is last_index,5)
  // sorted_match_index[] = {1 2 3 4 5}
  // in this case N is 3
  if (last_log_index > commit_index) {
    std::vector<int> sorted_match_index{last_log_index};

    for (std::string node : nodes) {
      sorted_match_index.emplace_back(match_index[node]);
    }

    std::sort(sorted_match_index.begin(), sorted_match_index.end());

    int N = sorted_match_index[num_nodes / 2];
    assert(N <= last_log_index);

    if (N > commit_index && logger.get_term(N) == term) { set_commit_index(N); }
  }
}

void raft_provider::run() {
  mu.lock();

  int last_applied = kvs.get_last_applied();
  int commit_index = get_commit_index();
  int limit_index = std::min(commit_index, last_applied + MAX_APPLIED_NUM);

  for (int index = last_applied + 1; index <= limit_index; index++) {
    int t;
    std::string k, v;
    logger.get_log(index, t, k, v);
    kvs.apply(index, k, v);
  }

  switch (get_state()) {
    case raft_state::ready:
      abort();
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
  int last_index, last_term;
  logger.get_last_log(last_index, last_term);
  printf("state: %d, term: %d, last: %d, commit: %d, applied: %d \n",
         (int)get_state(), get_current_term(), last_index, commit_index,
         last_applied);
  mu.unlock();
}

void raft_provider::append_node(std::string addr) {
  assert(get_state() == raft_state::ready);
  tl::endpoint p = get_engine().lookup(addr);
  nodes.push_back(addr);
  node_to_handle[addr] = tl::provider_handle(p, RAFT_PROVIDER_ID);
  num_nodes++;
  assert(num_nodes == (int)nodes.size() + 1);
  std::cout << "append node " << addr << std::endl;
}

void raft_provider::start(std::vector<std::string> &addrs) {
  for (std::string addr : addrs) { append_node(addr); }
  become_follower();
  // begin to accept rpc
  mu.unlock();
}