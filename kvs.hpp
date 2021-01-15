#ifndef KVS_HPP
#define KVS_HPP

#include "types.hpp"
#include <map>

/**
 * 揮発性のステートマシン、std::mapを使った最も単純なKey-Value Store
 * 型はKey,Valueともに文字列型のみ
 */
class raft_kvs {
private:
  // 状態
  std::map<std::string,std::string> data;
  // index of highest log entry applied to state machine
  int last_applied;
public:
  raft_kvs();
  ~raft_kvs();
  
  // key,valueを適用
  void apply(int index,std::string key,std::string value);
  // 現在のステートを参照してkeyに対応するvalueを取り出す
  // raft_stateがleaderのときのみしか呼び出されない
  std::string get(std::string key);
  int get_last_applied();
};

#endif