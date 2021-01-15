#include <unistd.h>
#include <vector>
#include <iostream>
#include "types.hpp"

std::string raft_state_to_string(raft_state s) {
  switch(s) {
  case raft_state::ready:
    return "ready";
  case raft_state::follower:
    return "follower";
  case raft_state::candidate:
    return "candidate";
  case raft_state::leader:
    return "leader";
  }
  return "";
}


int main(int argc,char **argv) {
  tl::engine my_engine("tcp", THALLIUM_CLIENT_MODE);
  tl::remote_procedure client_put = my_engine.define(CLIENT_PUT_RPC_NAME);
  tl::remote_procedure client_get = my_engine.define(CLIENT_GET_RPC_NAME);
  tl::remote_procedure echo_state = my_engine.define(ECHO_STATE_RPC_NAME);
  std::vector<std::string> nodes;

  while(1) {
    int opt = getopt(argc,argv,"n:h");
    if(opt==-1) break;
    switch(opt) {
    case 'n':
      nodes.push_back(optarg);
      break;
    case 'h':
      printf("Usage: \n %s [-n other_node1_addr] [-n other_node2_addr]\n",argv[0]);
      return -1;
      break;
    }
  }
  std::string cmd_buf,key_buf,value_buf;
  while(1) {

    cmd_buf.clear();
    key_buf.clear();
    value_buf.clear();

    while(!(cmd_buf=="put"||cmd_buf=="get")) {
      printf("enter cmd (put or get) \n>");
      std::cin >> cmd_buf;
    }

    printf("enter key name \n>");
    std::cin >> key_buf;
    
    if(cmd_buf=="put") {
      printf("enter value \n>");
      std::cin >> value_buf;
    }

    tl::endpoint leader;
    while (leader.is_null()) {
      printf("search leader node...\n");
      for(std::string node:nodes) {
        tl::endpoint p = my_engine.lookup(node);
        raft_state s = echo_state.on(p)();
        printf("node %s's state is %s\n",node.c_str(),raft_state_to_string(s).c_str());
        if(s==raft_state::leader) {
          leader = p;
          break;
        }
      }
      if(leader.is_null()) {
        sleep(3);
      };
    }
    
    if(cmd_buf=="put") {
      client_put.on(leader)(key_buf,value_buf);
      printf("put key: %s value: %s\n",key_buf.c_str(),value_buf.c_str());
    } else {
      std::string value = client_get.on(leader)(key_buf);
      printf("get %s is %s\n",key_buf.c_str(),value.c_str());
    }
    
  }
}