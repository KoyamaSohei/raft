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
  
  setup_segset(&ss);

  ABT_init(argc,argv);
  
  ABT_xstream_create(ABT_SCHED_NULL,&sig_stream);
  ABT_thread_create_on_xstream(sig_stream,signal_handler,&ss,ABT_THREAD_ATTR_NULL,&sig_thread);

  tl::engine my_engine("tcp", THALLIUM_SERVER_MODE);
  std::cout << "Server running at address " << my_engine.self() << std::endl;
  raft_provider provider(my_engine);

  if(argc > 1) {
    for(int i=1;i < argc;i++) {
      provider.append_node(argv[i]);
    }
  }
  
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