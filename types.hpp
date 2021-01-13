#ifndef TYPES_HPP
#define TYPES_HPP

class append_entries_request {
private:
  int term;

public:
  append_entries_request(int _term=1)
  : term(_term) {}

  template<typename A>
  void serialize(A& ar) {
    ar & term;
  }
};

class append_entries_response {
private:
  int term;
  bool success;
public:
  append_entries_response(int _term=1,bool _success=false)
  : term(_term), success(_success) {}

  int get_term() {
    return term;
  }

  bool is_success() {
    return success;
  }

  template<typename A>
  void serialize(A& ar) {
    ar & term;
    ar & success;
  }
};
#endif