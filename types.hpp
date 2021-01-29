#ifndef TYPES_HPP
#define TYPES_HPP

#include <uuid/uuid.h>

#include <cassert>
#include <string>
#include <thallium.hpp>
#include <thallium/serialization/stl/string.hpp>
#include <thallium/serialization/stl/vector.hpp>

#define INTERVAL 2000000
#define TIMEOUT_SPAN 2
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

struct raft_entry {
  int index;
  int term;
  std::string uuid;
  std::string command;

  raft_entry(int _index = 0, int _term = 0, std::string _uuid = "",
             std::string _command = "")
    : index(_index), term(_term), uuid(_uuid), command(_command) {
    assert(0 <= index);
  }

  template <typename A>
  void serialize(A& ar) {
    ar& index;
    ar& term;
    ar& uuid;
    ar& command;
  }
};

struct append_entries_response {
  int term;
  bool success;

  append_entries_response(int _term = 1, bool _success = false)
    : term(_term), success(_success) {
    assert(0 <= term);
  }

  template <typename A>
  void serialize(A& ar) {
    ar& term;
    ar& success;
  }
};

struct request_vote_response {
  int term;
  bool vote_granted;

  request_vote_response(int _term = 1, bool _vote_granted = false)
    : term(_term), vote_granted(_vote_granted) {
    assert(0 <= term);
  }

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
#define RAFT_DENY_REQUEST -10005
#define RAFT_SUCCESS 0
#define RAFT_FAILED 1

struct client_request_response {
  int status;
  std::string leader_hint;

  client_request_response(int _status = RAFT_NOT_IMPLEMENTED,
                          std::string _leader_hint = "")
    : status(_status), leader_hint(_leader_hint) {}

  template <typename A>
  void serialize(A& ar) {
    ar& status;
    ar& leader_hint;
  }
};

struct client_query_response {
  int status;
  std::string response;
  std::string leader_hint;

  client_query_response(int _status = RAFT_NOT_IMPLEMENTED,
                        std::string _response = "",
                        std::string _leader_hint = "")
    : status(_status), response(_response), leader_hint(_leader_hint) {}

  template <typename A>
  void serialize(A& ar) {
    ar& status;
    ar& response;
    ar& leader_hint;
  }
};

struct add_server_response {
  int status;
  std::string leader_hint;

  add_server_response(int _status = RAFT_NOT_IMPLEMENTED,
                      std::string _leader_hint = "")
    : status(_status), leader_hint(_leader_hint) {}

  template <typename A>
  void serialize(A& ar) {
    ar& status;
    ar& leader_hint;
  }
};

struct remove_server_response {
  int status;
  std::string leader_hint;

  remove_server_response(int _status = RAFT_NOT_IMPLEMENTED,
                         std::string _leader_hint = "")
    : status(_status), leader_hint(_leader_hint) {}

  template <typename A>
  void serialize(A& ar) {
    ar& status;
    ar& leader_hint;
  }
};

#define UUID_LENGTH 37

enum class raft_logger_mode {
  init,
  join,
  bootstrap,
};

#endif