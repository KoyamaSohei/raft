#ifndef FSM_HPP
#define FSM_HPP

#include <map>

#include "types.hpp"

/**
 *  raft_fsm manages Finite State Machine.
 *  After log is committed, raft_provider will apply that log to raft_fsm.
 *  raft_fsm parse commands and apply them.
 *  And, raft_provider ask raft_fsm to resolve query.
 *  raft_fsm resolve query with current state,
 *  which is consist of already applied log.
 *  command query, and query's answer type is std::string,
 *  but its content depends on the implementation.
 */
class raft_fsm {
public:
  raft_fsm(){};
  virtual ~raft_fsm(){};

  /**
   *  apply applies command.
   *  Important!:
   *    some commands may be EMPTY.
   *    (e.g. on become leader, leader append empty log)
   *    apply() have to handle empty command.
   *  @param command
   */
  virtual void apply(std::string command) = 0;

  /**
   *  resolve resolves query
   *  @param query
   *  @return answer
   */
  virtual std::string resolve(std::string query) = 0;
};

class kvs_raft_fsm : public raft_fsm {
private:
  std::unordered_map<std::string, std::string> data;

public:
  kvs_raft_fsm();
  ~kvs_raft_fsm();
  void apply(std::string command);
  std::string resolve(std::string query);
};

#endif