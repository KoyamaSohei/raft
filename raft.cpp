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
  printf("Usage: \n");
  printf("%s init [self addr]\n", argv[0]);
  printf("%s bootstrap [self addr]\n", argv[0]);
  printf("%s join [self addr] [target addr]\n", argv[0]);
  printf("For examples,\n");
  printf("%s init 127.0.0.1:30000\n", argv[0]);
}

void run_init(std::string self) {
  ABT_xstream sig_stream, tick_stream;
  ABT_thread sig_thread, tick_thread;
  ABT_thread_state tick_state;
  static sigset_t ss;

  setup_sigset(&ss);

  lmdb_raft_logger logger(self);
  kvs_raft_fsm fsm;

  ABT_init(0, NULL);
  logger.init();

  printf("try binding with %s%s\n", PROTOCOL_PREFIX, self.c_str());
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
}

void run_join(std::string self, std::string target_id) {
  ABT_xstream sig_stream, tick_stream;
  ABT_thread sig_thread, tick_thread;
  ABT_thread_state tick_state;
  static sigset_t ss;

  setup_sigset(&ss);

  ABT_init(0, NULL);

  printf("try binding with %s%s\n", PROTOCOL_PREFIX, self.c_str());
  tl::engine my_engine(PROTOCOL_PREFIX + self, THALLIUM_SERVER_MODE, true, 2);
  printf("Server running at address %s\n", ((string)my_engine.self()).c_str());

  tl::remote_procedure m_add_server_rpc(my_engine.define("add_server"));

  while (1) {
    tl::provider_handle ph(my_engine.lookup(target_id), RAFT_PROVIDER_ID);
    add_server_response resp = m_add_server_rpc.on(ph)(self);
    switch (resp.status) {
      case RAFT_LEADER_NOT_FOUND:
        printf("leader not found, please retry another addr\n");
        exit(0);
        break;
      case RAFT_NODE_IS_NOT_LEADER:
        target_id = resp.leader_hint;
      case RAFT_DENY_REQUEST:
        printf("deny request, please retry another addr\n");
        exit(0);
        break;
    }
    if (resp.status == RAFT_SUCCESS) { break; }
    usleep(INTERVAL);
  }

  lmdb_raft_logger logger(self);
  kvs_raft_fsm fsm;

  logger.join();

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
}

void run_bootstrap(std::string self) {
  ABT_xstream sig_stream, tick_stream;
  ABT_thread sig_thread, tick_thread;
  ABT_thread_state tick_state;
  static sigset_t ss;

  setup_sigset(&ss);

  lmdb_raft_logger logger(self);
  kvs_raft_fsm fsm;

  ABT_init(0, NULL);
  logger.bootstrap();

  printf("try binding with %s%s\n", PROTOCOL_PREFIX, self.c_str());
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