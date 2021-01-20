#include <string.h>
#include <unistd.h>

#include <iostream>
#include <vector>

#include "types.hpp"

void usage(int argc, char **argv) {
  printf("Usage: \n");
  printf("%s get [key]         [one of nodes addr]\n", argv[0]);
  printf("%s put [key] [value] [one of nodes addr]\n", argv[0]);
}

int main(int argc, char **argv) {
  tl::engine my_engine("sockets", THALLIUM_CLIENT_MODE);
  tl::remote_procedure client_put = my_engine.define(CLIENT_PUT_RPC_NAME);
  tl::remote_procedure client_get = my_engine.define(CLIENT_GET_RPC_NAME);

  if (argc <= 3) {
    usage(argc, argv);
    return 1;
  }

  if (strcasecmp(argv[1], "get") && strcasecmp(argv[1], "put")) {
    usage(argc, argv);
    return 1;
  }

  if (!strcasecmp(argv[1], "get")) {
    if (argc != 4) {
      usage(argc, argv);
      return 1;
    }

    std::string key = argv[2];
    tl::provider_handle handle(my_engine.lookup(argv[3]), RAFT_PROVIDER_ID);

    client_get_response resp = client_get.on(handle)(key);

    if (resp.get_error() == RAFT_SUCCESS) {
      std::cout << resp.get_value() << std::endl;
      return 0;
    }

    return resp.get_error();

  } else if (!strcasecmp(argv[1], "put")) {
    if (argc != 5) {
      usage(argc, argv);
      return 1;
    }

    std::string key = argv[2];
    std::string value = argv[3];
    tl::provider_handle handle(my_engine.lookup(argv[4]), RAFT_PROVIDER_ID);

    client_put_response resp = client_put.on(handle)(key, value);

    std::cout << resp.get_index() << std::endl;
    return resp.get_error();
  }
}