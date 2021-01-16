#include "kvs.hpp"
#include <cassert>

raft_kvs::raft_kvs()
  : last_applied(0)
{
  data["hello"]="world";
}

raft_kvs::~raft_kvs() {

}

void raft_kvs::apply(int index,std::string key,std::string value) {
  assert(index==last_applied+1);
  last_applied++;
  data[key]=value;
}

std::string raft_kvs::get(std::string key) {
  return data[key];
}

int raft_kvs::get_last_applied() {
  return last_applied;
}