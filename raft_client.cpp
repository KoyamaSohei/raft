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

    tl::provider_handle leader_handle;
    while (leader_handle.is_null()) {
      printf("search leader node...\n");
      for(std::string node:nodes) {
        printf("check if node %s is leader\n",node.c_str());
        tl::endpoint p = my_engine.lookup(node);
        // https://mochi.readthedocs.io/en/latest/thallium/12_rpc_pool.html
        // This feature requires to provide a non-zero provider id (passed to the define call) when defining the RPC (here 1). 
        // Hence you also need to use provider handles on clients, even if you do not define a provider class.
        tl::provider_handle handle(p,RAFT_PROVIDER_ID);
        int num = echo_state.on(handle)();;
        raft_state s = raft_state(num);
        printf("node %s's state is %s\n",node.c_str(),raft_state_to_string(s).c_str());
        if(s==raft_state::leader) {
          leader_handle = handle;
          break;
        }
      }
      if(leader_handle.is_null()) {
        sleep(3);
      };
    }
    
    if(cmd_buf=="put") {
      client_put.on(leader_handle)(key_buf,value_buf);
      printf("put key: %s value: %s\n",key_buf.c_str(),value_buf.c_str());
    } else {
      client_get_response resp = client_get.on(leader_handle)(key_buf);
      printf("get %s is %s\n",key_buf.c_str(),resp.get_value().c_str());
    }
    
  }
}