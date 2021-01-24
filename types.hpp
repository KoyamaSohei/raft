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
#define PROTOCOL_PREFIX "ofi+sockets://"

namespace tl = thallium;
using system_clock = std::chrono::system_clock;

enum class raft_state {
  follower,
  candidate,
  leader,
};

class raft_entry {
private:
  int index;
  int term;
  std::string uuid;
  std::string command;

public:
  raft_entry(int _index = 0, int _term = 0, std::string _uuid = "",
             std::string _command = "")
    : index(_index), term(_term), uuid(_uuid), command(_command) {
    assert(0 <= index);
  }

  int get_index() { return index; }

  int get_term() { return term; }

  std::string get_uuid() { return uuid; }

  std::string get_command() { return command; }

  template <typename A>
  void serialize(A& ar) {
    ar& index;
    ar& term;
    ar& uuid;
    ar& command;
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

#define CLIENT_REQUEST_RPC_NAME "client_request"
#define CLIENT_QUERY_RPC_NAME "client_query"

#define ECHO_STATE_RPC_NAME "echo_state"

#define RAFT_NODE_IS_NOT_FOLLOWER -9998
#define RAFT_NODE_IS_NOT_LEADER -9999
#define RAFT_NOT_IMPLEMENTED -10000
#define RAFT_LEADER_NOT_FOUND -10001
#define RAFT_DUPLICATE_UUID -10002
#define RAFT_INVALID_UUID -10003
#define RAFT_INVALID_REQUEST -10004
#define RAFT_SUCCESS 0
#define RAFT_FAILED 1

class client_request_response {
private:
  int status;
  int index;
  std::string leader_hint;

public:
  client_request_response(int _status = RAFT_NOT_IMPLEMENTED, int _index = 0,
                          std::string _leader_hint = "")
    : status(_status), index(_index), leader_hint(_leader_hint) {}

  int get_status() { return status; }

  int get_index() { return index; }

  std::string get_leader_hint() { return leader_hint; }

  template <typename A>
  void serialize(A& ar) {
    ar& status;
    ar& index;
    ar& leader_hint;
  }
};

class client_query_response {
private:
  int status;
  std::string response;
  std::string leader_hint;

public:
  client_query_response(int _status = RAFT_NOT_IMPLEMENTED,
                        std::string _response = "",
                        std::string _leader_hint = "")
    : status(_status), response(_response), leader_hint(_leader_hint) {}

  int get_status() { return status; }

  std::string get_response() { return response; }

  std::string get_leader_hint() { return leader_hint; }

  template <typename A>
  void serialize(A& ar) {
    ar& status;
    ar& response;
    ar& leader_hint;
  }
};

#define UUID_LENGTH 37

#define SPECIAL_ENTRY_KEY "__cluster"

#endif