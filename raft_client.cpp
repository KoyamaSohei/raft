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

void usage(int argc,char **argv) {
  printf("Usage: \n %s -n other_node1_addr [-n other_node2_addr]\n",argv[0]);
  printf("raft_client has 3 modes \n");
  printf("--------------------\n");
  printf("1. intaractive mode\n");
  printf("$ %s -n 'ofi+tcp;ofi_rxm://127.0.0.1:30000' -n 'ofi+tcp;ofi_rxm://127.0.0.1:31000'\n",argv[0]);
  printf("enter cmd (put or get)\n");
  printf(">get\n");
  printf("enter key name\n");
  printf(">hello\n");
  printf("search leader node...\n");
  printf("check if node ofi+tcp;ofi_rxm://127.0.0.1:30000 is leader\n");
  printf("node ofi+tcp;ofi_rxm://127.0.0.1:30000's state is leader\n");
  printf("get SUCCESS key: hello value: world\n");
  printf("--------------------\n");
  printf("2. get mode\n");
  printf("$ %s -g -k hello -n 'ofi+tcp;ofi_rxm://127.0.0.1:30000' -n 'ofi+tcp;ofi_rxm://127.0.0.1:31000'\n",argv[0]);
  printf("check if node ofi+tcp;ofi_rxm://127.0.0.1:30000 is leader\n");
  printf("node ofi+tcp;ofi_rxm://127.0.0.1:30000's state is leader\n");
  printf("get SUCCESS key: hello value: world\n");
  printf("--------------------\n");
  printf("3. put mode\n");
  printf("$ %s -p -k hello -v world -n 'ofi+tcp;ofi_rxm://127.0.0.1:30000' -n 'ofi+tcp;ofi_rxm://127.0.0.1:31000'\n",argv[0]);
  printf("check if node ofi+tcp;ofi_rxm://127.0.0.1:30000 is leader\n");
  printf("node ofi+tcp;ofi_rxm://127.0.0.1:30000's state is leader\n");
  printf("put SUCCESS key: hello value: world\n");
}


int main(int argc,char **argv) {
  tl::engine my_engine("tcp", THALLIUM_CLIENT_MODE);
  tl::remote_procedure client_put = my_engine.define(CLIENT_PUT_RPC_NAME);
  tl::remote_procedure client_get = my_engine.define(CLIENT_GET_RPC_NAME);
  tl::remote_procedure echo_state = my_engine.define(ECHO_STATE_RPC_NAME);
  std::vector<std::string> nodes;
  std::string cmd_buf,key_buf,value_buf;

  while(1) {
    int opt = getopt(argc,argv,"n:pgk:v:h");
    if(opt==-1) break;
    switch(opt) {
    case 'n':
      nodes.push_back(optarg);
      break;
    case 'p':
      cmd_buf = "put";
      break;
    case 'g':
      cmd_buf = "get";
      break;
    case 'k':
      key_buf = optarg;
      break;
    case 'v':
      value_buf = optarg;
      break;
    case 'h':
      usage(argc,argv);
      return -1;
      break;
    }
  }

  if(nodes.empty()) {
    printf("please set nodes\n");
    usage(argc,argv);
    exit(1);
  }
  
  if(cmd_buf.empty()) {
    printf("enter cmd (put or get) \n>");
    std::cin >> cmd_buf;
    if(!(cmd_buf=="put" || cmd_buf=="get")) {
      printf("cmd %s is invalid\n",cmd_buf.c_str());
      exit(1);
    }
  }

  if(key_buf.empty()) {
    printf("enter key name \n>");
    std::cin >> key_buf;
  }

  if(cmd_buf=="put" && value_buf.empty()) {
    printf("enter value \n>");
    std::cin >> value_buf;
  }
  
  if(cmd_buf=="put") {
    tl::provider_handle handle(my_engine.lookup(nodes[0]),RAFT_PROVIDER_ID);
    int resp = client_put.on(handle)(key_buf,value_buf);
    if(resp==RAFT_SUCCESS) {
      printf("put SUCCESS key: %s value: %s\n",key_buf.c_str(),value_buf.c_str());
      exit(0);
    } else if(resp==RAFT_NODE_IS_NOT_LEADER) {
      printf("put error because raft is not leader\n");
      exit(1);
    } else {
      printf("put error\n");
      exit(1);
    }
  } else {
    tl::provider_handle handle(my_engine.lookup(nodes[0]),RAFT_PROVIDER_ID);
    client_get_response resp = client_get.on(handle)(key_buf);
    if(resp.get_error()==RAFT_SUCCESS) {
      printf("get SUCCESS key: %s value: %s\n",key_buf.c_str(),resp.get_value().c_str());
      exit(0);
    } else if(resp.get_error()==RAFT_NODE_IS_NOT_LEADER) {
      printf("get error because raft is not leader\n");
      exit(1);
    } else {
      printf("get error\n");
      exit(1);
    }
  }
}