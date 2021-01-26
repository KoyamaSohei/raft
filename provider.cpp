#include "provider.hpp"

#include <unistd.h>

#include <cassert>

#include "builder.hpp"

raft_provider::raft_provider(tl::engine &e, raft_logger *_logger,
                             raft_fsm *_fsm, uint16_t provider_id)
  : tl::provider<raft_provider>(e, provider_id)
  , _state(raft_state::follower)
  , logger(_logger)
  , fsm(_fsm)
  , _commit_index(0)
  , _last_applied(0)
  , m_append_entries_rpc(
      define("append_entries", &raft_provider::append_entries_rpc))
  , m_request_vote_rpc(define("request_vote", &raft_provider::request_vote_rpc))
  , m_timeout_now_rpc(define("timeout_now", &raft_provider::timeout_now_rpc))
  , m_client_request_rpc(
      define(CLIENT_REQUEST_RPC_NAME, &raft_provider::client_request_rpc))
  , m_client_query_rpc(
      define(CLIENT_QUERY_RPC_NAME, &raft_provider::client_query_rpc))
  , m_add_server_rpc(define("add_server", &raft_provider::add_server_rpc))
  , m_remove_server_rpc(
      define("remove_server", &raft_provider::remove_server_rpc)) {
  define(ECHO_STATE_RPC_NAME, &raft_provider::echo_state_rpc);
  // Block RPC until start
  mu.lock();
}

raft_provider::~raft_provider() {}

void raft_provider::finalize() {
  leader_hint.clear();
  _node_to_handle.clear();
  m_append_entries_rpc.deregister();
  m_request_vote_rpc.deregister();
  m_client_request_rpc.deregister();
  m_client_query_rpc.deregister();
  get_engine().finalize();
}

raft_state raft_provider::get_state() {
  return _state;
}

void raft_provider::set_state(raft_state new_state) {
  switch (new_state) {
    case raft_state::follower:
      assert(_state == raft_state::candidate || _state == raft_state::leader);
      break;
    case raft_state::candidate:
      assert(_state == raft_state::follower || _state == raft_state::candidate);
      logger->set_current_term(logger->get_current_term() + 1);
      logger->set_voted_for_self();
      break;
    case raft_state::leader:
      assert(_state == raft_state::candidate);
      break;
    default:
      abort();
  }
  _state = new_state;
  leader_hint.clear();
}

int raft_provider::get_commit_index() {
  assert(0 <= _commit_index);
  return _commit_index;
}

void raft_provider::set_commit_index(int index) {
  assert(_commit_index < index);
  _commit_index = index;
}

int raft_provider::get_last_applied() {
  return _last_applied;
}

void raft_provider::set_last_applied(int index) {
  _last_applied = index;
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
    case raft_state::leader:
      abort();
  }
  timeout_limit =
    system_clock::now() + std::chrono::microseconds(span * INTERVAL);
}

void raft_provider::set_force_current_term(int term) {
  assert(logger->get_current_term() < term);
  logger->set_current_term(term);
  logger->clear_voted_for();

  switch (_state) {
    case raft_state::follower:
      break;
    case raft_state::candidate:
    case raft_state::leader:
      _state = raft_state::follower;
      update_timeout_limit();
      break;
  }
}

tl::provider_handle &raft_provider::get_handle(const std::string &node) {
  if (_node_to_handle.count(node)) { return _node_to_handle[node]; }

  std::string addr(PROTOCOL_PREFIX);
  addr += node;
  _node_to_handle[node] =
    tl::provider_handle(get_engine().lookup(addr), RAFT_PROVIDER_ID);
  return _node_to_handle[node];
}

// match_index initialized to 0
int raft_provider::get_match_index(const std::string &node) {
  if (_match_index.count(node)) { return _match_index[node]; }
  _match_index[node] = 0;
  return _match_index[node];
}

void raft_provider::set_match_index(const std::string &node, int index) {
  _match_index[node] = index;
}

// next_index initialized to leader last log index + 1
int raft_provider::get_next_index(const std::string &node) {
  if (_next_index.count(node)) { return _next_index[node]; }
  int last_index, last_term;
  logger->get_last_log(last_index, last_term);
  _next_index[node] = last_index + 1;
  return _next_index[node];
}

void raft_provider::set_next_index(const std::string &node, int index) {
  _next_index[node] = index;
}

void raft_provider::append_entries_rpc(const tl::request &r, int req_term,
                                       int req_prev_index, int req_prev_term,
                                       std::vector<raft_entry> req_entries,
                                       int req_leader_commit,
                                       std::string req_leader_id) {
  mu.lock();

  int current_term = logger->get_current_term();

  if (req_term < current_term) {
    mu.unlock();
    try {
      r.respond(append_entries_response(current_term, false));
    } catch (tl::exception &e) {
      printf("error respond to %s\n", std::string(r.get_endpoint()).c_str());
    }
    return;
  }

  leader_hint = req_leader_id;

  if (req_term > current_term) {
    set_force_current_term(req_term);
    current_term = req_term;
    assert(get_state() == raft_state::follower);
  }

  switch (get_state()) {
    case raft_state::candidate:
      become_follower();
      break;
    case raft_state::leader:
      printf("there are 2 leader in same term\n");
      abort();
      return;
    case raft_state::follower:
      // run below
      break;
  }

  assert(get_state() == raft_state::follower);
  update_timeout_limit();

  bool is_match = logger->match_log(req_prev_index, req_prev_term);
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
    printf("entry received, idx: %d, term: %d, cmd: %s\n", ent.get_index(),
           ent.get_term(), ent.get_command().c_str());
    logger->set_log(ent.get_index(), ent.get_term(), ent.get_uuid(),
                    ent.get_command());
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
  int current_term = logger->get_current_term();

  printf("request_vote_rpc from %s in term %d\n", req_candidate_id.c_str(),
         req_term);

  int last_log_index, last_log_term;
  logger->get_last_log(last_log_index, last_log_term);

  bool granted = [&]() -> bool {
    if (req_term < current_term) return false;
    if (req_term > current_term) {
      set_force_current_term(req_term);
      current_term = req_term;
    }
    if (logger->exists_voted_for()) return false;
    if (req_last_log_term < last_log_term) return false;
    if (req_last_log_term > last_log_term) return true;
    if (req_last_log_index < last_log_index) return false;
    return true;
  }();

  if (granted) {
    assert(!logger->exists_voted_for());
    logger->set_voted_for(req_candidate_id);
    update_timeout_limit();
  }
  mu.unlock();
  try {
    r.respond(request_vote_response(current_term, granted));
  } catch (tl::exception &e) {
    printf("error respond to %s\n", std::string(r.get_endpoint()).c_str());
  }
  return;
}

void raft_provider::timeout_now_rpc(const tl::request &r, int req_term,
                                    int req_prev_index, int req_prev_term) {
  mu.lock();
  if (get_state() != raft_state::follower) {
    mu.unlock();
    try {
      r.respond(RAFT_NODE_IS_NOT_FOLLOWER);
    } catch (tl::exception &e) {}
    return;
  }
  if (req_term != logger->get_current_term()) {
    mu.unlock();
    try {
      r.respond(RAFT_INVALID_REQUEST);
    } catch (tl::exception &e) {}
    return;
  }

  int last_log_index, last_log_term;
  logger->get_last_log(last_log_index, last_log_term);

  if (!(last_log_index == req_prev_index && last_log_term == req_prev_term)) {
    mu.unlock();
    try {
      r.respond(RAFT_INVALID_REQUEST);
    } catch (tl::exception &e) {}
    return;
  }
  update_timeout_limit();
  become_candidate();
  int err = (get_state() == raft_state::leader) ? RAFT_SUCCESS : RAFT_FAILED;
  mu.unlock();
  try {
    r.respond(err);
  } catch (tl::exception &e) {}
  return;
}

void raft_provider::client_request_rpc(const tl::request &r, std::string uuid,
                                       std::string command) {
  std::unique_lock<tl::mutex> lock(mu);
  if (get_state() != raft_state::leader) {
    if (leader_hint.empty()) {
      try {
        r.respond(client_request_response(RAFT_LEADER_NOT_FOUND));
      } catch (tl::exception &e) {}
      return;
    }
    try {
      r.respond(
        client_request_response(RAFT_NODE_IS_NOT_LEADER, 0, leader_hint));
    } catch (tl::exception &e) {}
    return;
  }
  if (uuid.size() + 1 != UUID_LENGTH) {
    try {
      r.respond(client_request_response(RAFT_INVALID_UUID, 0));
    } catch (tl::exception &e) {}
    return;
  }
  if (logger->contains_uuid(uuid)) {
    try {
      r.respond(client_request_response(RAFT_DUPLICATE_UUID, 0));
    } catch (tl::exception &e) {}
    return;
  }
  int index = logger->append_log(uuid, command);

  while (get_commit_index() < index && get_state() == raft_state::leader) {
    cond.wait(lock);
  }

  if (get_state() != raft_state::leader) {
    try {
      r.respond(
        client_request_response(RAFT_NODE_IS_NOT_LEADER, 0, leader_hint));
    } catch (tl::exception &e) {}
    return;
  }
  try {
    r.respond(client_request_response(RAFT_SUCCESS, index));
  } catch (tl::exception &e) {}
}

void raft_provider::client_query_rpc(const tl::request &r, std::string query) {
  std::unique_lock<tl::mutex> lock(mu);
  if (get_state() != raft_state::leader) {
    if (leader_hint.empty()) {
      try {
        r.respond(client_query_response(RAFT_LEADER_NOT_FOUND, ""));
      } catch (tl::exception &e) {}
      return;
    }
    try {
      r.respond(
        client_query_response(RAFT_NODE_IS_NOT_LEADER, "", leader_hint));
    } catch (tl::exception &e) {}
    return;
  }
  try {
    r.respond(client_query_response(RAFT_SUCCESS, fsm->resolve(query)));
  } catch (tl::exception &e) {}
}

void raft_provider::add_server_rpc(const tl::request &r,
                                   std::string new_server) {
  mu.lock();
  if (get_state() != raft_state::leader) {
    if (!leader_hint.empty()) {
      mu.unlock();
      try {
        r.respond(add_server_response(RAFT_LEADER_NOT_FOUND, ""));
      } catch (tl::exception &e) {}
      return;
    }
    mu.unlock();
    try {
      r.respond(add_server_response(RAFT_NODE_IS_NOT_LEADER, leader_hint));
    } catch (tl::exception &e) {}
    return;
  }
  if (logger->get_last_conf_applied() < get_commit_index()) {
    mu.unlock();
    try {
      r.respond(add_server_response(RAFT_DENY_REQUEST, leader_hint));
    } catch (tl::exception &e) {}
    return;
  }
  if (new_server == logger->get_id()) {
    mu.unlock();
    try {
      r.respond(add_server_response(RAFT_DENY_REQUEST, leader_hint));
    } catch (tl::exception &e) {}
    return;
  }
  std::string uuid;
  generate_special_uuid(uuid);
  logger->set_add_conf_log(logger->get_current_term(), uuid, new_server);
  mu.unlock();
  try {
    r.respond(add_server_response(RAFT_SUCCESS, leader_hint));
  } catch (tl::exception &e) {}
  return;
}

void raft_provider::remove_server_rpc(const tl::request &r,
                                      std::string old_server) {
  mu.lock();
  if (get_state() != raft_state::leader) {
    if (!leader_hint.empty()) {
      mu.unlock();
      try {
        r.respond(add_server_response(RAFT_LEADER_NOT_FOUND, ""));
      } catch (tl::exception &e) {}
      return;
    }
    mu.unlock();
    try {
      r.respond(add_server_response(RAFT_NODE_IS_NOT_LEADER, leader_hint));
    } catch (tl::exception &e) {}
    return;
  }
  if (logger->get_last_conf_applied() < get_commit_index()) {
    mu.unlock();
    try {
      r.respond(add_server_response(RAFT_DENY_REQUEST, leader_hint));
    } catch (tl::exception &e) {}
    return;
  }
  if (old_server == logger->get_id()) {
    mu.unlock();
    try {
      r.respond(add_server_response(RAFT_DENY_REQUEST, leader_hint));
    } catch (tl::exception &e) {}
    return;
  }
  std::string uuid;
  generate_special_uuid(uuid);
  logger->set_remove_conf_log(logger->get_current_term(), uuid, old_server);
  mu.unlock();
  try {
    r.respond(add_server_response(RAFT_SUCCESS, leader_hint));
  } catch (tl::exception &e) {}
  return;
}

void raft_provider::echo_state_rpc(const tl::request &r) {
  try {
    r.respond((int)get_state());
  } catch (tl::exception &e) {}
}

void raft_provider::become_follower() {
  printf("become follower\n");
  update_timeout_limit();
  set_state(raft_state::follower);
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
  logger->get_last_log(last_log_index, last_log_term);
  int current_term = logger->get_current_term();

  int vote = 1;

  for (std::string node : logger->get_peers()) {
    mu.unlock();
    printf("request_vote to %s\n", node.c_str());
    request_vote_response resp;
    try {
      resp = m_request_vote_rpc.on(get_handle(node))(
        current_term, logger->get_id(), last_log_index, last_log_term);
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
  if (vote * 2 > logger->get_num_nodes()) {
    become_leader();
    return;
  }
}

void raft_provider::run_candidate() {
  if (system_clock::now() > timeout_limit) {
    update_timeout_limit();
    become_candidate();
  }
}

void raft_provider::become_leader() {
  printf("become leader\n");

  set_state(raft_state::leader);
  std::string uuid;
  generate_uuid(uuid);
  logger->append_log(uuid, "");
  _next_index.clear();
  _match_index.clear();
}

void raft_provider::run_leader() {
  int term = logger->get_current_term();
  int commit_index = get_commit_index();
  int last_log_index, _;
  logger->get_last_log(last_log_index, _);

  for (std::string node : logger->get_peers()) {
    int prev_index = get_next_index(node) - 1;
    assert(0 <= prev_index);
    int prev_term = logger->get_term(prev_index);
    std::vector<raft_entry> entries;
    int last_index =
      std::min(last_log_index, get_next_index(node) + MAX_ENTRIES_NUM);
    for (int idx = get_next_index(node); idx <= last_index; idx++) {
      int t;
      std::string uuid, command;
      logger->get_log(idx, t, uuid, command);
      entries.emplace_back(idx, t, uuid, command);
    }

    mu.unlock();
    append_entries_response resp;
    try {
      resp = m_append_entries_rpc.on(get_handle(node))(
        term, prev_index, prev_term, entries, commit_index, logger->get_id());
    } catch (const tl::exception &e) {
      printf("error occured at node %s\n", node.c_str());
      mu.lock();
      continue;
    }
    mu.lock();
    if (get_state() == raft_state::follower) { return; }
    assert(get_state() == raft_state::leader);

    if (resp.get_term() > logger->get_current_term()) {
      become_follower();
      return;
    }

    if (resp.is_success()) {
      set_match_index(node, last_index);
      set_next_index(node, last_index + 1);
    } else {
      set_next_index(node, get_next_index(node) - 1);
      assert(get_next_index(node) > 0);
    }

    printf("node %s match: %d, next: %d\n", node.c_str(), get_match_index(node),
           get_next_index(node));
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

    for (std::string node : logger->get_peers()) {
      sorted_match_index.emplace_back(get_match_index(node));
    }

    std::sort(sorted_match_index.begin(), sorted_match_index.end());

    int N = sorted_match_index[logger->get_num_nodes() / 2];
    assert(N <= last_log_index);

    if (N > commit_index && logger->get_term(N) == term) {
      set_commit_index(N);
    }
  }
}

void raft_provider::run() {
  mu.lock();

  int last_applied = get_last_applied();
  int commit_index = get_commit_index();
  int limit_index = std::min(commit_index, last_applied + MAX_APPLIED_NUM);

  for (int index = last_applied + 1; index <= limit_index; index++) {
    int t;
    std::string uuid, command;
    logger->get_log(index, t, uuid, command);
    if (!uuid_is_special(uuid)) { fsm->apply(command); }
    set_last_applied(index);
  }

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
  int last_index, last_term;
  logger->get_last_log(last_index, last_term);
  printf("state: %d, term: %d, last: %d, commit: %d, applied: %d \n",
         (int)get_state(), logger->get_current_term(), last_index, commit_index,
         last_applied);
  mu.unlock();
  cond.notify_all();
}

void raft_provider::start() {
  update_timeout_limit();
  // begin to accept rpc
  mu.unlock();
}

void raft_provider::transfer_leadership() {
  assert(get_state() == raft_state::leader);
  assert(logger->get_num_nodes() > 1);

  int current_term = logger->get_current_term();
  int commit_index = get_commit_index();

  std::string target;

  int match_idx = 0;

  for (std::string node : logger->get_peers()) {
    if (get_match_index(node) >= match_idx) {
      target = node;
      match_idx = get_match_index(node);
    }
  }

  if (match_idx < commit_index) {
    printf("candidate of next leader not found\n");
    return;
  }

  assert(get_match_index(target) == match_idx);
  assert(!target.empty());

  printf("begin transfer leadership to %s\n", target.c_str());

  int last_log_index, last_log_term;
  logger->get_last_log(last_log_index, last_log_term);

  bool target_has_latest_log = match_idx == last_log_index;

  if (!target_has_latest_log) {
    //  Send Append Entries RPC
    int prev_index = get_next_index(target) - 1;

    assert(0 <= prev_index);
    assert(prev_index <= last_log_index);

    int prev_term = logger->get_term(prev_index);

    std::vector<raft_entry> entries;
    for (int idx = get_next_index(target); idx <= last_log_index; idx++) {
      int t;
      std::string uuid, command;
      logger->get_log(idx, t, uuid, command);
      entries.emplace_back(idx, t, uuid, command);
    }

    try {
      append_entries_response resp = m_append_entries_rpc.on(
        get_handle(target))(current_term, prev_index, prev_term, entries,
                            commit_index, logger->get_id());

      if (resp.get_term() > current_term) {
        printf("target has greater term\n");
        become_follower();
        return;
      }

      if (resp.is_success()) { target_has_latest_log = true; }

    } catch (tl::exception &e) {}
  }
  if (!target_has_latest_log) {
    printf("target has NOT latest log,transfer leadership failed\n");
    return;
  }

  try {
    int err = m_timeout_now_rpc.on(get_handle(target))(
      current_term, last_log_index, last_log_term);

    if (err == RAFT_SUCCESS) {
      printf("transfer leadership succeeded, please retry.\n");
    } else {
      printf("error occured on transfer leadership, please retry\n");
    }
  } catch (tl::exception &e) {}
  return;
}

bool raft_provider::remove_self_from_cluster() {
  mu.lock();
  if (logger->get_num_nodes() == 1) {
    mu.unlock();
    return true;
  }
  if (get_state() == raft_state::leader) {
    transfer_leadership();
    mu.unlock();
    return false;
  }
  if (!logger->contains_self_in_nodes() &&
      logger->get_last_conf_applied() <= get_commit_index()) {
    mu.unlock();
    printf("self id is not exists in nodes,shutdown...\n");
    return true;
  }
  if (leader_hint.empty()) {
    mu.unlock();
    printf("leader not found, please retry after elected new leader\n");
    return false;
  }
  mu.unlock();
  try {
    remove_server_response resp =
      m_remove_server_rpc.on(get_handle(leader_hint))(logger->get_id());
    if (resp.get_status() == RAFT_SUCCESS) {
      printf("successfly  sending remove_server rpc,please wait\n");
      return false;
    }
    if (resp.get_status() == RAFT_NODE_IS_NOT_LEADER) {
      leader_hint = resp.get_leader_hint();
      printf("leader is seemed changed, please retry\n");
      return false;
    }
    printf("error occured on leader, please retry\n");
  } catch (tl::exception &e) {
    printf("error occured on sending remove_server rpc, please retry\n");
    return false;
  }

  return false;
}