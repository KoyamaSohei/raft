#ifndef TYPES_HPP
#define TYPES_HPP

#include <thallium.hpp>
#include <thallium/serialization/stl/string.hpp>
#include <string>

#define INTERVAL 5
#define TIMEOUT  10

namespace tl = thallium;
using system_clock = std::chrono::system_clock;

enum class raft_state {
  ready,
  follower,
  candidate,
  leader,
};

class raft_entry {
private:
  int index;
  std::string key;
  std::string value;
public:
  raft_entry(int _index=0,std::string _key="",std::string _value="")
  : index(_index),key(_key),value(_value) {}

  int get_index() {
    return index;
  }

  std::string get_key() {
    return key;
  }

  std::string get_value() {
    return value;
  }

  template<typename A>
  void serialize(A& ar) {
    ar & index;
    ar & key;
    ar & value;
  }
};

class append_entries_request {
private:
  int term;
  int prev_index;
  int prev_term;
  std::vector<raft_entry> entries;
  int leader_commit;
public:
  append_entries_request(
    int _term=1,
    int _prev_index=0,
    int _prev_term=0,
    std::vector<raft_entry> _entries=std::vector<raft_entry>(),
    int _leader_commit=0)
  : term(_term),
    prev_index(_prev_index),
    prev_term(_prev_term),
    entries(_entries),
    leader_commit(_leader_commit)
  {}

  int get_term() {
    return term;
  }

  int get_prev_index() {
    return prev_index;
  }

  int get_prev_term() {
    return prev_term;
  }

  std::vector<raft_entry> get_entries() {
    return entries;
  }

  int get_leader_commit() {
    return leader_commit;
  }

  template<typename A>
  void serialize(A& ar) {
    ar & term;
  }
};

class append_entries_response {
private:
  int term;
  bool success;
public:
  append_entries_response(int _term=1,bool _success=false)
  : term(_term), success(_success) {}

  int get_term() {
    return term;
  }

  bool is_success() {
    return success;
  }

  template<typename A>
  void serialize(A& ar) {
    ar & term;
    ar & success;
  }
};

class request_vote_request {
private:
  int term;
  std::string candidate_id;
public:
  request_vote_request(int _term=1,tl::endpoint _candidate_id=tl::endpoint())
  : term(_term), candidate_id(std::string(_candidate_id)) {}

  int get_term() {
    return term;
  }

  std::string get_candidate_id() {
    return candidate_id;
  }

  template<typename A>
  void serialize(A& ar) {
    ar & term;
    ar & candidate_id;
  }
};

class request_vote_response {
private:
  int term;
  bool vote_granted;
public:
  request_vote_response(int _term=1,bool _vote_granted=false)
  : term(_term), vote_granted(_vote_granted) {}

  int get_term() {
    return term;
  }

  bool is_vote_granted() {
    return vote_granted;
  }

  template<typename A>
  void serialize(A& ar) {
    ar & term;
    ar & vote_granted;
  }
};

#define CLIENT_PUT_RPC_NAME "client_put"
#define CLIENT_GET_RPC_NAME "client_get"

#define ECHO_STATE_RPC_NAME "echo_state"

#define RAFT_NODE_IS_NOT_LEADER -9999
#define RAFT_NOT_IMPLEMENTED    -10000

class client_get_response {
private:
  int error;
  std::string value;
public:
  client_get_response(int _error=RAFT_NOT_IMPLEMENTED,std::string _value="")
  : error(_error), value(_value) {}

  int get_error() {
    return error;
  }

  std::string get_value() {
    return value;
  }

  template<typename A>
  void serialize(A& ar) {
    ar & error;
    ar & value;
  }
};

#endif