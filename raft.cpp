#include <iostream>
#include <thallium.hpp>

namespace tl = thallium;

class RaftProvider : public tl::provider<RaftProvider> {
public:
  RaftProvider(tl::engine& e,uint16_t provider_id=1)
  : tl::provider<RaftProvider>(e, provider_id) {
    get_engine().push_finalize_callback(this,[p=this]() {delete p;});
  }
  ~RaftProvider() {
    get_engine().pop_finalize_callback(this);
  }
};

int main(int argc, char** argv) {
  tl::engine myEngine("tcp", THALLIUM_SERVER_MODE);
  std::cout << "Server running at address " << myEngine.self() << std::endl;
  RaftProvider provider(myEngine);
  myEngine.wait_for_finalize();
  return 0;
}