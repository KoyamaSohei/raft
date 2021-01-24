#include <json/json.h>
#include <string.h>
#include <unistd.h>

#include <iostream>
#include <random>
#include <vector>

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

std::string generate_id() {
  uuid_t id;
  uuid_generate(id);
  char sid[UUID_LENGTH];
  uuid_unparse_lower(id, sid);
  return std::string(sid);
}

void get_nodes_from_buf(std::string buf, std::vector<std::string> &nodes) {
  if (buf.empty()) { return; }
  std::string::size_type pos = 0, next;

  do {
    next = buf.find(",", pos);
    nodes.emplace_back(buf.substr(pos, next - pos));
    pos = next + 1;
  } while (next != std::string::npos);
}

int main(int argc, char **argv) {
  tl::engine my_engine(PROTOCOL_PREFIX, THALLIUM_CLIENT_MODE);
  tl::remote_procedure request = my_engine.define(CLIENT_REQUEST_RPC_NAME);
  tl::remote_procedure query = my_engine.define(CLIENT_QUERY_RPC_NAME);
  std::vector<std::string> nodes;
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
    get_nodes_from_buf(argv[3], nodes);

    auto get_radom_node = [&]() { return nodes[rnd() % nodes.size()]; };

    std::string next_addr = get_radom_node();

    while (1) {

      tl::endpoint e = my_engine.lookup(next_addr);
      tl::provider_handle ph(e, RAFT_PROVIDER_ID);
      client_query_response resp = query.on(ph)(command);

      int status = resp.get_status();

      switch (status) {
        case RAFT_SUCCESS:
          std::cout << resp.get_response() << std::endl;
          return 0;
        case RAFT_NODE_IS_NOT_LEADER:
          next_addr = resp.get_leader_hint();
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

    std::string key = argv[2];
    std::string value = argv[3];

    Json::Value root;
    root["key"] = key;
    root["value"] = value;
    Json::StreamWriterBuilder builder;
    std::string command = Json::writeString(builder, root);

    std::string uuid = generate_id();

    get_nodes_from_buf(argv[4], nodes);

    auto get_radom_node = [&]() { return nodes[rnd() % nodes.size()]; };

    std::string next_addr = get_radom_node();

    while (1) {
      tl::endpoint e = my_engine.lookup(next_addr);
      tl::provider_handle ph(e, RAFT_PROVIDER_ID);
      client_request_response resp = request.on(ph)(uuid, command);

      int status = resp.get_status();
      switch (status) {
        case RAFT_SUCCESS:
          std::cout << resp.get_index() << std::endl;
          return 0;
        case RAFT_NODE_IS_NOT_LEADER:
          next_addr = resp.get_leader_hint();
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