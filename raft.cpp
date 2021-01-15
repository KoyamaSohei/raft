#include <iostream>
#include <thallium.hpp>
#include <abt.h>
#include <unistd.h>
#include <cassert>
#include "provider.hpp"

void signal_handler(void *arg) {
  int num;
  sigwait((sigset_t *)arg,&num);
  std::cout << "Signal received " << num << std::endl;
  exit(1);
}

void tick_loop(void *provider) {
  printf("tick!\n");
  ((raft_provider *)provider)->run();
}

void setup_segset(sigset_t *ss) {
  sigemptyset(ss);
  sigaddset(ss, SIGINT);
  sigaddset(ss, SIGTERM);
  pthread_sigmask(SIG_BLOCK, ss, NULL);
}

int main(int argc, char** argv) {

  ABT_xstream sig_stream,tick_stream;
  ABT_thread sig_thread,tick_thread;
  ABT_thread_state tick_state;
  static sigset_t ss;

  std::string self_addr = "tcp";
  std::vector<std::string> nodes;
  
  setup_segset(&ss);

  ABT_init(argc,argv);
  
  ABT_xstream_create(ABT_SCHED_NULL,&sig_stream);
  ABT_thread_create_on_xstream(sig_stream,signal_handler,&ss,ABT_THREAD_ATTR_NULL,&sig_thread);

  while(1) {
    int opt = getopt(argc,argv, "s:n:h");
    if(opt==-1) break;
    switch(opt) {
    case 's':
      self_addr = optarg;
      break;
    case 'n':
      nodes.push_back(optarg);
      break;
    case 'h':
      printf("Usage: \n %s [-s self_addr] [-n other_node1_addr] [-n other_node2_addr]\n",argv[0]);
      return -1;
      break;
    }
  }

  tl::engine my_engine(self_addr, THALLIUM_SERVER_MODE,true,2);
  std::cout << "Server running at address " << my_engine.self() << std::endl;
  raft_provider provider(my_engine);
  
  provider.start(nodes);
  
  ABT_xstream_create(ABT_SCHED_NULL,&tick_stream);
  ABT_thread_create_on_xstream(tick_stream,tick_loop,&provider,ABT_THREAD_ATTR_NULL,&tick_thread);

  while(1) {
    sleep(INTERVAL);
    ABT_thread_get_state(tick_thread,&tick_state);
    assert(tick_state==ABT_THREAD_STATE_TERMINATED);
    ABT_thread_free(&tick_thread);
    ABT_thread_create_on_xstream(tick_stream,tick_loop,&provider,ABT_THREAD_ATTR_NULL,&tick_thread);
  }

  my_engine.wait_for_finalize();

  ABT_thread_free(&sig_thread);
  ABT_thread_free(&tick_thread);

  ABT_xstream_free(&sig_stream);
  ABT_xstream_free(&tick_stream);

  return 0;
}