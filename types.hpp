#ifndef TYPES_HPP
#define TYPES_HPP

#include <thallium.hpp>
#include <thallium/serialization/stl/string.hpp>
#include <string>

namespace tl = thallium;

enum class raft_state {
  follower,
  candidate,
  leader,
};

class append_entries_request {
private:
  int term;

public:
  append_entries_request(int _term=1)
  : term(_term) {}

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

#endif