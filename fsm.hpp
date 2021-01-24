#ifndef FSM_HPP
#define FSM_HPP

#include <map>

#include "types.hpp"

class raft_fsm {
public:
  raft_fsm(){};
  virtual ~raft_fsm(){};

  // apply command
  virtual void apply(std::string command) = 0;
  // resolve query
  virtual std::string resolve(std::string query) = 0;
};

class kvs_raft_fsm : public raft_fsm {
private:
  std::map<std::string, std::string> data;

public:
  kvs_raft_fsm();
  ~kvs_raft_fsm();
  void apply(std::string command);
  std::string resolve(std::string query);
};

#endif