#include <abt.h>
#include <unistd.h>

#include <cassert>
#include <iostream>
#include <thallium.hpp>
#include <thread>

#include "builder.hpp"
#include "provider.hpp"

using namespace std;

void setup_sigset(sigset_t *ss) {
  sigemptyset(ss);
  sigaddset(ss, SIGINT);
  sigaddset(ss, SIGTERM);
  pthread_sigmask(SIG_BLOCK, ss, NULL);
}

struct signal_handler_arg_t {
  sigset_t *ss;
  raft_provider *provider;
};

void *signal_handler(void *arg) {
  int num;
  bool ok = false;
  while (!ok) {
    sigwait(((signal_handler_arg_t *)arg)->ss, &num);
    std::cout << "Signal received " << num << std::endl;
    bool ok =
      ((signal_handler_arg_t *)arg)->provider->remove_self_from_cluster();
  }
}

void usage(int argc, char **argv) {
  printf("Usage: \n");
  printf("%s init [self addr]\n", argv[0]);
  printf("%s bootstrap [self addr]\n", argv[0]);
  printf("%s join [self addr] [target addr]\n", argv[0]);
  printf("For examples,\n");
  printf("%s init 127.0.0.1:30000\n", argv[0]);
}

void run_init(std::string self) {
  pthread_t signal_thread;
  static sigset_t ss;

  setup_sigset(&ss);

  {
    printf("try binding with %s%s\n", PROTOCOL_PREFIX, self.c_str());
    tl::engine my_engine(PROTOCOL_PREFIX + self, THALLIUM_SERVER_MODE, true, 2);
    printf("Server running at address %s\n",
           ((string)my_engine.self()).c_str());

    lmdb_raft_logger logger(self, raft_logger_mode::init);
    kvs_raft_fsm fsm;

    raft_provider provider(my_engine, &logger, &fsm);

    signal_handler_arg_t arg{.ss = &ss, .provider = &provider};

    if (pthread_create(&signal_thread, NULL, signal_handler, NULL)) {
      printf("error creating thread.");
      abort();
    }
  }

  if (pthread_join(signal_thread, NULL)) {
    printf("error joining thread.\n");
    abort();
  }
}

void run_join(std::string self, std::string target_id) {
  pthread_t signal_thread;
  static sigset_t ss;

  setup_sigset(&ss);

  {
    printf("try binding with %s%s\n", PROTOCOL_PREFIX, self.c_str());
    tl::engine my_engine(PROTOCOL_PREFIX + self, THALLIUM_SERVER_MODE, true, 2);
    printf("Server running at address %s\n",
           ((string)my_engine.self()).c_str());

    tl::remote_procedure m_add_server_rpc(my_engine.define("add_server"));

    while (1) {
      tl::provider_handle ph(my_engine.lookup(PROTOCOL_PREFIX + target_id),
                             RAFT_PROVIDER_ID);
      add_server_response resp = m_add_server_rpc.on(ph)(self);
      switch (resp.status) {
        case RAFT_LEADER_NOT_FOUND:
          printf("leader not found, please retry another addr\n");
          exit(0);
          break;
        case RAFT_NODE_IS_NOT_LEADER:
          target_id = resp.leader_hint;
          break;
        case RAFT_DENY_REQUEST:
          printf("deny request, please retry another addr\n");
          exit(0);
          break;
      }
      if (resp.status == RAFT_SUCCESS) { break; }
      usleep(INTERVAL);
    }

    lmdb_raft_logger logger(self, raft_logger_mode::join);
    kvs_raft_fsm fsm;

    raft_provider provider(my_engine, &logger, &fsm);

    signal_handler_arg_t arg{.ss = &ss, .provider = &provider};

    if (pthread_create(&signal_thread, NULL, signal_handler, NULL)) {
      printf("error creating thread.");
      abort();
    }
  }

  if (pthread_join(signal_thread, NULL)) {
    printf("error joining thread.\n");
    abort();
  }
}

void run_bootstrap(std::string self) {
  pthread_t signal_thread;
  static sigset_t ss;

  setup_sigset(&ss);
  {
    printf("try binding with %s%s\n", PROTOCOL_PREFIX, self.c_str());
    tl::engine my_engine(PROTOCOL_PREFIX + self, THALLIUM_SERVER_MODE, true, 2);
    printf("Server running at address %s\n",
           ((string)my_engine.self()).c_str());

    lmdb_raft_logger logger(self, raft_logger_mode::bootstrap);
    kvs_raft_fsm fsm;

    raft_provider provider(my_engine, &logger, &fsm);

    signal_handler_arg_t arg{.ss = &ss, .provider = &provider};

    if (pthread_create(&signal_thread, NULL, signal_handler, NULL)) {
      printf("error creating thread.");
      abort();
    }
  }

  if (pthread_join(signal_thread, NULL)) {
    printf("error joining thread.\n");
    abort();
  }
}

int main(int argc, char **argv) {

  if (argc < 3) {
    usage(argc, argv);
    return 1;
  }

  if (!strcasecmp(argv[1], "init")) {
    if (argc != 3) {
      usage(argc, argv);
      return 1;
    }
    run_init(argv[2]);
    return 0;
  }

  if (!strcasecmp(argv[1], "bootstrap")) {
    if (argc != 3) {
      usage(argc, argv);
      return 1;
    }
    run_bootstrap(argv[2]);
    return 0;
  }

  if (!strcasecmp(argv[1], "join")) {
    if (argc != 4) {
      usage(argc, argv);
      return 1;
    }
    run_join(argv[2], argv[3]);
    return 0;
  }
  usage(argc, argv);
  return 1;
}