#include <abt.h>
#include <unistd.h>

#include <cassert>
#include <iostream>
#include <thallium.hpp>

#include "builder.hpp"
#include "provider.hpp"

using namespace std;

struct signal_handler_arg_t {
  sigset_t *ss;
  raft_provider *provider;
};

void signal_handler(void *arg) {
  int num;
  while (1) {
    sigwait(((signal_handler_arg_t *)arg)->ss, &num);
    std::cout << "Signal received " << num << std::endl;
    bool ok =
      ((signal_handler_arg_t *)arg)->provider->remove_self_from_cluster();
    if (ok) { break; }
  }

  ((signal_handler_arg_t *)arg)->provider->finalize();
  exit(0);
}

void tick_loop(void *provider) {
  ((raft_provider *)provider)->run();
}

void setup_sigset(sigset_t *ss) {
  sigemptyset(ss);
  sigaddset(ss, SIGINT);
  sigaddset(ss, SIGTERM);
  pthread_sigmask(SIG_BLOCK, ss, NULL);
}

void usage(int argc, char **argv) {
  printf("Basic usage: \n");
  printf(
    "%s '127.0.0.1:30000' "
    "'127.0.0.1:30000"
    ",127.0.0.1:30001"
    ",127.0.0.1:30002' \n",
    argv[0]);
  printf(
    "In this case, we have to run :30001 and :30002 node in other process , "
    "same host\n");
  printf("And this program binds 127.0.0.1:30000\n");
}

void setup_nodes(int argc, char **argv, string &self, set<string> &nodes) {
  if (argc != 3) {
    usage(argc, argv);
    exit(1);
  }
  self = argv[1];
  get_set_from_seq(nodes, argv[2]);

  if (!nodes.count(self)) {
    printf("please add self address to nodes\n");
    usage(argc, argv);
    exit(1);
  }
}

int main(int argc, char **argv) {

  ABT_xstream sig_stream, tick_stream;
  ABT_thread sig_thread, tick_thread;
  ABT_thread_state tick_state;
  static sigset_t ss;
  string self;
  set<string> nodes;

  setup_nodes(argc, argv, self, nodes);
  setup_sigset(&ss);

  lmdb_raft_logger logger(self, nodes);
  kvs_raft_fsm fsm;

  ABT_init(argc, argv);
  logger.init();

  printf("try binding with %s%s\n", PROTOCOL_PREFIX, self);
  tl::engine my_engine(PROTOCOL_PREFIX + self, THALLIUM_SERVER_MODE, true, 2);
  printf("Server running at address %s\n", ((string)my_engine.self()).c_str());

  raft_provider provider(my_engine, &logger, &fsm);

  signal_handler_arg_t arg{.ss = &ss, .provider = &provider};

  ABT_xstream_create(ABT_SCHED_NULL, &sig_stream);
  ABT_thread_create_on_xstream(sig_stream, signal_handler, &arg,
                               ABT_THREAD_ATTR_NULL, &sig_thread);

  provider.start();

  ABT_xstream_create(ABT_SCHED_NULL, &tick_stream);
  ABT_thread_create_on_xstream(tick_stream, tick_loop, &provider,
                               ABT_THREAD_ATTR_NULL, &tick_thread);

  while (1) {
    usleep(INTERVAL);
    ABT_thread_get_state(tick_thread, &tick_state);
    assert(tick_state == ABT_THREAD_STATE_TERMINATED);
    ABT_thread_free(&tick_thread);
    ABT_thread_create_on_xstream(tick_stream, tick_loop, &provider,
                                 ABT_THREAD_ATTR_NULL, &tick_thread);
  }

  my_engine.wait_for_finalize();

  ABT_thread_free(&sig_thread);
  ABT_thread_free(&tick_thread);

  ABT_xstream_free(&sig_stream);
  ABT_xstream_free(&tick_stream);

  return 0;
}