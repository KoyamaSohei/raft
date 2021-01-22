#ifndef TYPES_HPP
#define TYPES_HPP

#include <uuid/uuid.h>

#include <cassert>
#include <string>
#include <thallium.hpp>
#include <thallium/serialization/stl/string.hpp>
#include <thallium/serialization/stl/vector.hpp>

#define INTERVAL 5000000
#define RAFT_PROVIDER_ID 999
#define MAX_ENTRIES_NUM 100
#define MAX_APPLIED_NUM 100

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
  int term;
  std::string uuid;
  std::string key;
  std::string value;

public:
  raft_entry(int _index = 0, int _term = 0, std::string _uuid = "",
             std::string _key = "", std::string _value = "")
    : index(_index), term(_term), uuid(_uuid), key(_key), value(_value) {
    assert(0 <= index);
  }

  int get_index() { return index; }

  int get_term() { return term; }

  std::string get_uuid() { return uuid; }

  std::string get_key() { return key; }

  std::string get_value() { return value; }

  template <typename A>
  void serialize(A& ar) {
    ar& index;
    ar& term;
    ar& uuid;
    ar& key;
    ar& value;
  }
};

class append_entries_response {
private:
  int term;
  bool success;

public:
  append_entries_response(int _term = 1, bool _success = false)
    : term(_term), success(_success) {
    assert(0 <= term);
  }

  int get_term() { return term; }

  bool is_success() { return success; }

  template <typename A>
  void serialize(A& ar) {
    ar& term;
    ar& success;
  }
};

class request_vote_response {
private:
  int term;
  bool vote_granted;

public:
  request_vote_response(int _term = 1, bool _vote_granted = false)
    : term(_term), vote_granted(_vote_granted) {
    assert(0 <= term);
  }

  int get_term() { return term; }

  bool is_vote_granted() { return vote_granted; }

  template <typename A>
  void serialize(A& ar) {
    ar& term;
    ar& vote_granted;
  }
};

#define CLIENT_PUT_RPC_NAME "client_put"
#define CLIENT_GET_RPC_NAME "client_get"

#define ECHO_STATE_RPC_NAME "echo_state"

#define RAFT_NODE_IS_NOT_LEADER -9999
#define RAFT_NOT_IMPLEMENTED -10000
#define RAFT_LEADER_NOT_FOUND -10001
#define DUPLICATE_REQEST_ID -10002
#define RAFT_INVALID_UUID -10003
#define RAFT_SUCCESS 0

class client_put_response {
private:
  int error;
  int index;
  std::string leader_id;

public:
  client_put_response(int _error = RAFT_NOT_IMPLEMENTED, int _index = 0,
                      std::string _leader_id = "")
    : error(_error), index(_index), leader_id(_leader_id) {}

  int get_error() { return error; }

  int get_index() { return index; }

  std::string get_leader_id() { return leader_id; }

  template <typename A>
  void serialize(A& ar) {
    ar& error;
    ar& index;
    ar& leader_id;
  }
};

class client_get_response {
private:
  int error;
  std::string value;
  std::string leader_id;

public:
  client_get_response(int _error = RAFT_NOT_IMPLEMENTED,
                      std::string _value = "", std::string _leader_id = "")
    : error(_error), value(_value), leader_id(_leader_id) {}

  int get_error() { return error; }

  std::string get_value() { return value; }

  std::string get_leader_id() { return leader_id; }

  template <typename A>
  void serialize(A& ar) {
    ar& error;
    ar& value;
    ar& leader_id;
  }
};

#define UUID_LENGTH 37

#endif