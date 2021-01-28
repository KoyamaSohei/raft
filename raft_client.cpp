#include <string.h>
#include <unistd.h>

#include <iostream>
#include <random>
#include <set>

#include "builder.hpp"
#include "fsm.hpp"
#include "types.hpp"

void usage(int argc, char **argv) {
  printf("Usage: \n");
  printf("%s query   [command]     [nodes addr]\n", argv[0]);
  printf("%s request [key] [value] [nodes addr]\n", argv[0]);
  printf("For examples,\n");
  printf(
    "%s query hello "
    "'127.0.0.1:30000,127.0.0.1:30001,127.0.0.1:30002' \n",
    argv[0]);
}

int main(int argc, char **argv) {
  tl::engine my_engine(PROTOCOL_PREFIX, THALLIUM_CLIENT_MODE);
  tl::remote_procedure request = my_engine.define(CLIENT_REQUEST_RPC_NAME);
  tl::remote_procedure query = my_engine.define(CLIENT_QUERY_RPC_NAME);
  std::set<std::string> nodes;
  std::random_device rnd;

  if (argc <= 3) {
    usage(argc, argv);
    return 1;
  }

  if (strcasecmp(argv[1], "query") && strcasecmp(argv[1], "request")) {
    usage(argc, argv);
    return 1;
  }

  if (!strcasecmp(argv[1], "query")) {
    if (argc != 4) {
      usage(argc, argv);
      return 1;
    }

    std::string command = argv[2];
    get_set_from_seq(nodes, argv[3]);

    auto get_radom_node = [&]() {
      auto itr = std::next(nodes.begin(), rnd() % nodes.size());
      return *itr;
    };

    std::string next_addr = get_radom_node();

    while (1) {

      tl::endpoint e = my_engine.lookup(next_addr);
      tl::provider_handle ph(e, RAFT_PROVIDER_ID);
      client_query_response resp = query.on(ph)(command);

      switch (resp.status) {
        case RAFT_SUCCESS:
          std::cout << resp.response << std::endl;
          return 0;
        case RAFT_NODE_IS_NOT_LEADER:
          next_addr = resp.leader_hint;
          continue;
        case RAFT_LEADER_NOT_FOUND:
          std::cerr << "leader not found" << std::endl;
          next_addr = get_radom_node();
          continue;
        case RAFT_NOT_IMPLEMENTED:
          std::cerr << "not implemented" << std::endl;
          abort();
        case RAFT_DUPLICATE_UUID:
          std::cerr << "duplicate request" << std::endl;
          abort();
        default:
          std::cerr << "unknown error" << std::endl;
          abort();
      }
    }

  } else if (!strcasecmp(argv[1], "request")) {
    if (argc != 5) {
      usage(argc, argv);
      return 1;
    }

    std::string command, uuid;
    kvs_raft_fsm::build_command(command, argv[2], argv[3]);
    generate_uuid(uuid);

    get_set_from_seq(nodes, argv[4]);

    auto get_radom_node = [&]() {
      auto itr = std::next(nodes.begin(), rnd() % nodes.size());
      return *itr;
    };

    std::string next_addr = get_radom_node();

    while (1) {
      tl::endpoint e = my_engine.lookup(next_addr);
      tl::provider_handle ph(e, RAFT_PROVIDER_ID);
      client_request_response resp = request.on(ph)(uuid, command);

      switch (resp.status) {
        case RAFT_SUCCESS:
          return 0;
        case RAFT_NODE_IS_NOT_LEADER:
          next_addr = resp.leader_hint;
          continue;
        case RAFT_LEADER_NOT_FOUND:
          std::cerr << "leader not found" << std::endl;
          next_addr = get_radom_node();
          continue;
        case RAFT_NOT_IMPLEMENTED:
          std::cerr << "not implemented" << std::endl;
          abort();
        case RAFT_DUPLICATE_UUID:
          std::cerr << "duplicate request" << std::endl;
          abort();
        default:
          std::cerr << "unknown error" << std::endl;
          abort();
      }
    }
  }
}