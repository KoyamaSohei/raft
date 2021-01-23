#include <abt.h>
#include <unistd.h>

#include <cassert>
#include <iostream>
#include <thallium.hpp>

#include "provider.hpp"

struct signal_handler_arg_t {
  sigset_t *ss;
  raft_provider *provider;
};

void signal_handler(void *arg) {
  int num;
  sigwait(((signal_handler_arg_t *)arg)->ss, &num);
  std::cout << "Signal received " << num << std::endl;
  ((signal_handler_arg_t *)arg)->provider->finalize();
  exit(1);
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

  ABT_xstream sig_stream, tick_stream;
  ABT_thread sig_thread, tick_thread;
  ABT_thread_state tick_state;
  static sigset_t ss;

  std::string self_addr = "sockets";
  std::string node_buf;
  std::vector<std::string> nodes;

  while (1) {
    int opt = getopt(argc, argv, "s:n:h");
    if (opt == -1) break;
    switch (opt) {
      case 's':
        self_addr = optarg;
        break;
      case 'n':
        node_buf = optarg;
        break;
      case 'h':
        printf(
          "Usage: \n %s [-s self_addr] [-n "
          "other_node1_addr,other_node2_addr]\n",
          argv[0]);
        return -1;
        break;
    }
  }

  get_nodes_from_buf(node_buf, nodes);

  setup_sigset(&ss);

  ABT_init(argc, argv);

  std::cout << "try binding with " << self_addr << std::endl;
  tl::engine my_engine(self_addr, THALLIUM_SERVER_MODE, true, 2);
  std::cout << "Server running at address " << my_engine.self() << std::endl;

  lmdb_raft_logger logger(my_engine.self());
  logger.init();

  raft_provider provider(my_engine, &logger, RAFT_PROVIDER_ID);

  signal_handler_arg_t arg{.ss = &ss, .provider = &provider};

  ABT_xstream_create(ABT_SCHED_NULL, &sig_stream);
  ABT_thread_create_on_xstream(sig_stream, signal_handler, &arg,
                               ABT_THREAD_ATTR_NULL, &sig_thread);

  provider.start(nodes);

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