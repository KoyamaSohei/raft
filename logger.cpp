#include "logger.hpp"

#include <errno.h>
#include <lmdb.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cassert>
#include <cereal/archives/json.hpp>
#include <cereal/cereal.hpp>
#include <cereal/types/set.hpp>
#include <string>

#include "builder.hpp"

void raft_logger::parse_log(int &term, std::string &uuid, std::string &command,
                            const std::string &src) {
  std::stringstream ss;
  { ss << src; }
  {
    cereal::JSONInputArchive archive(ss);
    archive(CEREAL_NVP(term), CEREAL_NVP(uuid), CEREAL_NVP(command));
  }
}

void raft_logger::build_log(std::string &dst, const int &term,
                            const std::string &uuid,
                            const std::string &command) {
  std::stringstream ss;
  {
    cereal::JSONOutputArchive archive(ss);
    archive(CEREAL_NVP(term), CEREAL_NVP(uuid), CEREAL_NVP(command));
  }
  dst = ss.str();
}

void raft_logger::parse_conf_log(int &prev_index,
                                 std::set<std::string> &prev_nodes,
                                 int &next_index,
                                 std::set<std::string> &next_nodes,
                                 const std::string &src) {
  std::stringstream ss;
  { ss << src; }
  {
    cereal::JSONInputArchive archive(ss);
    archive(CEREAL_NVP(prev_index), CEREAL_NVP(prev_nodes),
            CEREAL_NVP(next_index), CEREAL_NVP(next_nodes));
  }
}

void raft_logger::build_conf_log(std::string &dst, const int &prev_index,
                                 const std::set<std::string> &prev_nodes,
                                 const int &next_index,
                                 const std::set<std::string> &next_nodes) {
  std::stringstream ss;
  {
    cereal::JSONOutputArchive archive(ss);
    archive(CEREAL_NVP(prev_index), CEREAL_NVP(prev_nodes),
            CEREAL_NVP(next_index), CEREAL_NVP(next_nodes));
  }
  dst = ss.str();
}

lmdb_raft_logger::lmdb_raft_logger(std::string _id, raft_logger_mode mode)
  : raft_logger(_id, mode) {
  switch (mode) {
    case raft_logger_mode::init:
      init();
      break;
    case raft_logger_mode::join:
      join();
      break;
    case raft_logger_mode::bootstrap:
      bootstrap();
      break;
  }
}

lmdb_raft_logger::~lmdb_raft_logger() {}

void lmdb_raft_logger::init() {
  printf("init logger\n");
  MDB_txn *txn;
  MDB_dbi log_dbi, state_dbi;
  MDB_stat log_stat;
  std::string path = "log-" + id;
  int err;

  err = mkdir(path.c_str(), 0755);
  assert(err == 0 || errno == EEXIST);

  err = mdb_env_create(&env);
  assert(err == 0);

  err = mdb_env_set_maxdbs(env, 100);
  assert(err == 0);

  err = mdb_env_open(env, path.c_str(), 0, 0755);
  assert(err == 0);

  err = mdb_txn_begin(env, NULL, 0, &txn);
  assert(err == 0);

  err = mdb_dbi_open(txn, log_db, MDB_CREATE, &log_dbi);
  if (err) {
    mdb_txn_abort(txn);
    abort();
  }

  err = mdb_dbi_open(txn, state_db, MDB_CREATE, &state_dbi);
  if (err) {
    mdb_txn_abort(txn);
    abort();
  }

  err = mdb_stat(txn, log_dbi, &log_stat);
  if (err) {
    mdb_txn_abort(txn);
    abort();
  }
  assert(log_stat.ms_entries == 0);

  // Start init log
  // 1. append dummy log
  //    this log is dummy.
  //    to make `match index check` more simple
  // 2. append cluster config
  //     with nodes = {self}

  // index:0 term: 0
  stored_log_num = 1;
  int index = 0;
  std::string conf, uuid, log;
  generate_special_uuid(uuid);
  build_conf_log(conf, index, std::set<std::string>{}, index,
                 std::set<std::string>{});
  build_log(log, 0, uuid, conf);
  set_log_str(index, log, txn);

  // index:1 ,term: 0
  stored_log_num = 2;
  index = 1;
  nodes = std::set<std::string>{id};
  build_conf_log(conf, 0, std::set<std::string>{}, index, nodes);
  generate_special_uuid(uuid);
  build_log(log, 0, uuid, conf);
  set_log_str(index, log, txn);

  // save to state DB
  MDB_val last_conf_applied_value = {sizeof(int), (void *)&index};
  err = mdb_put(txn, state_dbi, &last_conf_applied_key,
                &last_conf_applied_value, 0);

  last_conf_applied = index;

  if (err) {
    mdb_txn_abort(txn);
    abort();
  }

  current_term = 0;

  MDB_val current_term_value = {sizeof(int), &current_term};

  err = mdb_put(txn, state_dbi, &current_term_key, &current_term_value, 0);
  if (err) {
    mdb_txn_abort(txn);
    abort();
  }

  voted_for = "";

  MDB_val voted_for_value = {sizeof(char) * (voted_for.size() + 1),
                             (void *)voted_for.c_str()};

  err = mdb_put(txn, state_dbi, &voted_for_key, &voted_for_value, 0);
  if (err) {
    mdb_txn_abort(txn);
    abort();
  }
  err = mdb_txn_commit(txn);
  if (err) {
    mdb_txn_abort(txn);
    abort();
  }
}

void lmdb_raft_logger::join() {
  printf("join logger\n");
  MDB_txn *txn;
  MDB_dbi log_dbi, state_dbi;
  MDB_stat log_stat;
  std::string path = "log-" + id;
  int err;

  err = mkdir(path.c_str(), 0755);
  assert(err == 0 || errno == EEXIST);

  err = mdb_env_create(&env);
  assert(err == 0);

  err = mdb_env_set_maxdbs(env, 100);
  assert(err == 0);

  err = mdb_env_open(env, path.c_str(), 0, 0755);
  assert(err == 0);

  err = mdb_txn_begin(env, NULL, 0, &txn);
  assert(err == 0);

  err = mdb_dbi_open(txn, log_db, MDB_CREATE, &log_dbi);
  if (err) {
    mdb_txn_abort(txn);
    abort();
  }

  err = mdb_dbi_open(txn, state_db, MDB_CREATE, &state_dbi);
  if (err) {
    mdb_txn_abort(txn);
    abort();
  }

  err = mdb_stat(txn, log_dbi, &log_stat);
  if (err) {
    mdb_txn_abort(txn);
    abort();
  }

  assert(log_stat.ms_entries == 0);

  // Start init log
  // 1. append dummy log
  //    this log is dummy.
  //    to make `match index check` more simple
  stored_log_num = 1;

  // index:0 term: 0 dummy
  int index = 0;
  std::string conf, uuid, log;
  generate_special_uuid(uuid);
  build_conf_log(conf, index, std::set<std::string>{}, index,
                 std::set<std::string>{});
  build_log(log, 0, uuid, conf);
  set_log_str(index, log, txn);

  // save to state DB
  MDB_val last_conf_applied_value = {sizeof(int), (void *)&index};
  err = mdb_put(txn, state_dbi, &last_conf_applied_key,
                &last_conf_applied_value, 0);

  last_conf_applied = index;

  if (err) {
    mdb_txn_abort(txn);
    abort();
  }

  current_term = 0;

  MDB_val current_term_value = {sizeof(int), &current_term};

  err = mdb_put(txn, state_dbi, &current_term_key, &current_term_value, 0);
  if (err) {
    mdb_txn_abort(txn);
    abort();
  }

  voted_for = "";

  MDB_val voted_for_value = {sizeof(char) * (voted_for.size() + 1),
                             (void *)voted_for.c_str()};

  err = mdb_put(txn, state_dbi, &voted_for_key, &voted_for_value, 0);
  if (err) {
    mdb_txn_abort(txn);
    abort();
  }
  err = mdb_txn_commit(txn);
  if (err) {
    mdb_txn_abort(txn);
    abort();
  }
}

void lmdb_raft_logger::bootstrap() {
  printf("bootstrap logger\n");
  MDB_txn *txn;
  MDB_dbi log_dbi, state_dbi;
  MDB_stat log_stat;
  std::string path = "log-" + id;
  int err;

  err = mkdir(path.c_str(), 0755);
  assert(err == 0 || errno == EEXIST);

  err = mdb_env_create(&env);
  assert(err == 0);

  err = mdb_env_set_maxdbs(env, 100);
  assert(err == 0);

  err = mdb_env_open(env, path.c_str(), 0, 0755);
  assert(err == 0);

  err = mdb_txn_begin(env, NULL, 0, &txn);
  assert(err == 0);

  err = mdb_dbi_open(txn, log_db, MDB_CREATE, &log_dbi);
  if (err) {
    mdb_txn_abort(txn);
    abort();
  }

  err = mdb_dbi_open(txn, state_db, MDB_CREATE, &state_dbi);
  if (err) {
    mdb_txn_abort(txn);
    abort();
  }

  err = mdb_stat(txn, log_dbi, &log_stat);
  if (err) {
    mdb_txn_abort(txn);
    abort();
  }

  assert(log_stat.ms_entries > 0);

  stored_log_num = log_stat.ms_entries;

  // recover state
  MDB_val current_term_value, voted_for_value, last_conf_applied_value;

  err = mdb_get(txn, state_dbi, &current_term_key, &current_term_value);
  if (err) {
    mdb_txn_abort(txn);
    abort();
  }
  current_term = *((int *)current_term_value.mv_data);

  err = mdb_get(txn, state_dbi, &voted_for_key, &voted_for_value);
  if (err) {
    mdb_txn_abort(txn);
    abort();
  }
  voted_for = std::string((char *)voted_for_value.mv_data);

  err =
    mdb_get(txn, state_dbi, &last_conf_applied_key, &last_conf_applied_value);
  if (err) {
    mdb_txn_abort(txn);
    abort();
  }

  last_conf_applied = *((int *)last_conf_applied_value.mv_data);
  assert(last_conf_applied < stored_log_num);

  std::string buf = get_log_str(last_conf_applied, txn);
  int term;
  std::string uuid, command;
  parse_log(term, uuid, command, buf);
  assert(uuid_is_special(uuid));

  int prev_index, next_index;
  std::set<std::string> prev_nodes, next_nodes;
  parse_conf_log(prev_index, prev_nodes, next_index, next_nodes, command);
  assert(next_index == last_conf_applied);
  nodes = peers = next_nodes;
  assert(nodes.count(id));
  peers.erase(id);

  printf("bootstrap: current_term is %d\n", current_term);
  printf("bootstrap: voted_for is %s\n", voted_for.c_str());
  printf("bootstrap: latest conf is %s\n", command.c_str());
  printf("bootstrap: stored_log_num is %d\n", stored_log_num);
  printf("bootstrap: last_conf_applied is %d\n", last_conf_applied);

  err = mdb_txn_commit(txn);
  if (err) {
    mdb_txn_abort(txn);
    abort();
  }
}

void lmdb_raft_logger::clean_up() {
  int err;
  std::string dir_path = "log-" + id;
  std::string data_path = dir_path + "/data.mdb";
  std::string lock_path = dir_path + "/lock.mdb";
  err = remove(data_path.c_str());
  assert(err == 0 || errno == ENOENT);

  err = remove(lock_path.c_str());
  assert(err == 0 || errno == ENOENT);

  err = rmdir(dir_path.c_str());
  assert(err == 0 || errno == ENOENT);
}

std::string lmdb_raft_logger::get_id() {
  return id;
}

std::set<std::string> &lmdb_raft_logger::get_peers() {
  return peers;
}

int lmdb_raft_logger::get_num_nodes() {
  return nodes.size();
}

bool lmdb_raft_logger::contains_self_in_nodes() {
  return nodes.count(id) > 0;
}

int lmdb_raft_logger::get_current_term() {
  return current_term;
}

void lmdb_raft_logger::set_current_term(int new_term) {
  assert(current_term < new_term);
  MDB_txn *txn;
  MDB_dbi dbi;
  MDB_val current_term_value;
  int err;

  err = mdb_txn_begin(env, NULL, 0, &txn);
  assert(err == 0);

  err = mdb_dbi_open(txn, state_db, MDB_CREATE, &dbi);
  if (err) {
    mdb_txn_abort(txn);
    abort();
  }

  current_term_value = {sizeof(int), &new_term};

  err = mdb_put(txn, dbi, &current_term_key, &current_term_value, 0);
  if (err) {
    mdb_txn_abort(txn);
    abort();
  }

  err = mdb_txn_commit(txn);
  if (err) {
    mdb_txn_abort(txn);
    abort();
  }

  current_term = new_term;
  voted_for = "";
}

bool lmdb_raft_logger::exists_voted_for() {
  return voted_for.size() > 0;
}

void lmdb_raft_logger::set_voted_for_self() {
  set_voted_for(id);
  voted_for.assign(id);
}

void lmdb_raft_logger::set_voted_for(const std::string &new_addr) {
  MDB_txn *txn;
  MDB_dbi dbi;
  MDB_val voted_for_value;
  int err;

  err = mdb_txn_begin(env, NULL, 0, &txn);
  assert(err == 0);

  err = mdb_dbi_open(txn, state_db, MDB_CREATE, &dbi);
  if (err) {
    mdb_txn_abort(txn);
    abort();
  }

  // voted_for.size() is not include '\0' so +1
  voted_for_value.mv_size = sizeof(char) * (new_addr.size() + 1);
  voted_for_value.mv_data = (void *)new_addr.c_str();

  err = mdb_put(txn, dbi, &voted_for_key, &voted_for_value, 0);
  if (err) {
    mdb_txn_abort(txn);
    abort();
  }

  err = mdb_txn_commit(txn);
  if (err) {
    mdb_txn_abort(txn);
    abort();
  }

  voted_for.clear();
  voted_for.assign(new_addr);
}

void lmdb_raft_logger::set_log_str(int index, std::string log_str,
                                   MDB_txn *ptxn) {
  assert(0 <= index);
  assert(index <= stored_log_num);
  MDB_txn *txn;
  MDB_stat stat;
  MDB_dbi dbi;
  MDB_val save_log_key, save_log_value;
  char save_log_key_buf[11];
  int err;

  err = mdb_txn_begin(env, ptxn, 0, &txn);
  assert(err == 0);

  err = mdb_dbi_open(txn, log_db, 0, &dbi);
  if (err) {
    mdb_txn_abort(txn);
    abort();
  }

  save_log_key.mv_size = 11;
  sprintf(save_log_key_buf, "%010d", index);
  save_log_key.mv_data = save_log_key_buf;

  // log_str.size() is not include '\0' so +1
  save_log_value.mv_size = sizeof(char) * (log_str.size() + 1);
  save_log_value.mv_data = (void *)log_str.c_str();

  err = mdb_put(txn, dbi, &save_log_key, &save_log_value, 0);
  if (err) {
    mdb_txn_abort(txn);
    abort();
  }

  err = mdb_stat(txn, dbi, &stat);
  if (err) {
    mdb_txn_abort(txn);
    abort();
  }

  assert(stored_log_num == (int)stat.ms_entries);

  err = mdb_txn_commit(txn);
  if (err) {
    mdb_txn_abort(txn);
    abort();
  }
}

void lmdb_raft_logger::set_uuid(std::string uuid, MDB_txn *ptxn) {
  MDB_txn *txn;
  MDB_dbi dbi;
  MDB_val save_uuid_key, save_uuid_value;
  int err;
  err = mdb_txn_begin(env, ptxn, 0, &txn);
  assert(err == 0);

  err = mdb_dbi_open(txn, uuid_db, MDB_CREATE, &dbi);
  if (err) {
    mdb_txn_abort(txn);
    abort();
  }

  save_uuid_key.mv_size = sizeof(char) * (uuid.size() + 1);
  save_uuid_key.mv_data = (void *)uuid.c_str();

  err = mdb_get(txn, dbi, &save_uuid_key, &save_uuid_value);

  if (err != MDB_NOTFOUND) {
    mdb_txn_abort(txn);
    abort();
  }

  save_uuid_value.mv_size = 0;
  save_uuid_value.mv_data = NULL;

  err = mdb_put(txn, dbi, &save_uuid_key, &save_uuid_value, 0);
  if (err) {
    mdb_txn_abort(txn);
    abort();
  }

  err = mdb_txn_commit(txn);
  if (err) {
    mdb_txn_abort(txn);
    abort();
  }
}

bool lmdb_raft_logger::get_uuid(const std::string &uuid, MDB_txn *ptxn) {
  MDB_txn *txn;
  MDB_dbi dbi;
  MDB_val get_uuid_key, get_uuid_value;
  int err;

  err = mdb_txn_begin(env, ptxn, 0, &txn);
  assert(err == 0);

  err = mdb_dbi_open(txn, uuid_db, MDB_CREATE, &dbi);
  if (err) {
    mdb_txn_abort(txn);
    abort();
  }

  get_uuid_key.mv_size = sizeof(char) * (uuid.size() + 1);
  get_uuid_key.mv_data = (void *)uuid.c_str();

  err = mdb_get(txn, dbi, &get_uuid_key, &get_uuid_value);

  if (err == MDB_NOTFOUND) {
    err = mdb_txn_commit(txn);
    if (err) {
      mdb_txn_abort(txn);
      abort();
    }
    return false;
  }

  if (err) {
    mdb_txn_abort(txn);
    abort();
  }

  err = mdb_txn_commit(txn);
  if (err) {
    mdb_txn_abort(txn);
    abort();
  }
  return true;
}

std::string lmdb_raft_logger::get_log_str(int index, MDB_txn *ptxn) {
  assert(0 <= index);
  if (index >= stored_log_num) {
    // not found
    return "";
  }
  MDB_txn *txn;
  MDB_dbi dbi;
  MDB_val get_log_key, get_log_value;
  char get_log_key_buf[11];
  int err;

  err = mdb_txn_begin(env, ptxn, 0, &txn);
  assert(err == 0);

  err = mdb_dbi_open(txn, log_db, 0, &dbi);
  if (err) {
    mdb_txn_abort(txn);
    abort();
  }

  get_log_key.mv_size = 11;
  sprintf(get_log_key_buf, "%010d", index);
  get_log_key.mv_data = get_log_key_buf;

  err = mdb_get(txn, dbi, &get_log_key, &get_log_value);
  if (err) {
    mdb_txn_abort(txn);
    abort();
  }

  err = mdb_txn_commit(txn);
  if (err) {
    mdb_txn_abort(txn);
    abort();
  }

  return std::string((char *)get_log_value.mv_data);
}

int lmdb_raft_logger::append_log(const std::string &uuid,
                                 const std::string &command) {
  int index = stored_log_num;
  int term = current_term;
  set_log(index, term, uuid, command);
  return index;
}

void lmdb_raft_logger::set_log(const int index, const int term,
                               const std::string &uuid,
                               const std::string &command) {
  assert(0 <= index);
  assert(index <= stored_log_num);
  if (index == stored_log_num) {
    // append log
    stored_log_num++;
  }
  std::string log;
  build_log(log, term, uuid, command);
  MDB_txn *txn;
  int err = mdb_txn_begin(env, NULL, 0, &txn);
  assert(err == 0);

  if (index == last_conf_applied) {
    // rollback config change
    printf("rollback config start\n");
    int p_term;
    std::string p_uuid, p_conf;

    std::string p_log = get_log_str(index, txn);
    parse_log(p_term, p_uuid, p_conf, p_log);

    assert(uuid_is_special(p_uuid));

    int p_prev_index, p_next_index;
    std::set<std::string> p_prev_nodes, p_next_nodes;
    parse_conf_log(p_prev_index, p_prev_nodes, p_next_index, p_next_nodes,
                   p_conf);
    printf("overwritten config %s \n", p_log.c_str());
    assert(p_next_index == index);

    // roll back
    last_conf_applied = p_prev_index;
    nodes = peers = p_prev_nodes;
    peers.erase(id);
  }

  set_log_str(index, log, txn);
  set_uuid(uuid, txn);

  if (!uuid_is_special(uuid)) {
    err = mdb_txn_commit(txn);
    if (err) {
      mdb_txn_abort(txn);
      abort();
    }
    printf("save log index: %d,term: %d, uuid: %s,cmd: %s \n", index, term,
           uuid.substr(0, 8).c_str(), command.c_str());
    return;
  }

  // apply config change

  MDB_dbi state_dbi;
  err = mdb_dbi_open(txn, state_db, MDB_CREATE, &state_dbi);
  if (err) {
    mdb_txn_abort(txn);
    abort();
  }

  MDB_val last_conf_applied_value = {sizeof(int), (void *)&index};
  err = mdb_put(txn, state_dbi, &last_conf_applied_key,
                &last_conf_applied_value, 0);
  if (err) {
    mdb_txn_abort(txn);
    abort();
  }

  err = mdb_txn_commit(txn);
  if (err) {
    mdb_txn_abort(txn);
    abort();
  }

  int prev_index, next_index;
  std::set<std::string> prev_nodes, next_nodes;
  parse_conf_log(prev_index, prev_nodes, next_index, next_nodes, command);

  assert(index == next_index);
  assert(prev_index == last_conf_applied);
  last_conf_applied = next_index;
  nodes = peers = next_nodes;
  if (peers.count(id)) { peers.erase(id); }

  printf("save special log \nconf: \n%s\n", command.c_str());
}

int lmdb_raft_logger::get_term(int index) {
  assert(0 <= index);
  assert(index <= stored_log_num - 1);
  int term;
  std::string uuid, command;
  get_log(index, term, uuid, command);
  return term;
}

void lmdb_raft_logger::get_log(int index, int &term, std::string &uuid,
                               std::string &command) {
  assert(0 <= index);
  assert(index <= stored_log_num - 1);
  std::string log = get_log_str(index);
  parse_log(term, uuid, command, log);
}

void lmdb_raft_logger::get_last_log(int &index, int &term) {
  index = stored_log_num - 1;
  std::string uuid, command;
  get_log(index, term, uuid, command);
  assert(0 <= index);
  assert(0 <= term);
}

bool lmdb_raft_logger::match_log(int index, int term) {
  assert(0 <= index);
  if (stored_log_num <= index) { return false; }
  assert(index < stored_log_num);
  int t = get_term(index);
  return t == term;
}

bool lmdb_raft_logger::contains_uuid(const std::string &uuid) {
  return get_uuid(uuid);
}

int lmdb_raft_logger::get_last_conf_applied() {
  assert(last_conf_applied >= 0);
  return last_conf_applied;
}

int lmdb_raft_logger::set_add_conf_log(const std::string &new_server) {
  assert(!nodes.count(new_server));
  int term = current_term;
  std::string uuid;
  generate_special_uuid(uuid);

  std::set<std::string> prev_nodes(nodes), next_nodes(nodes);
  next_nodes.insert(new_server);
  int prev_index = last_conf_applied;
  int next_index = stored_log_num;
  std::string conf;
  build_conf_log(conf, prev_index, prev_nodes, next_index, next_nodes);
  set_log(next_index, term, uuid, conf);
  return next_index;
}

int lmdb_raft_logger::set_remove_conf_log(const std::string &old_server) {
  assert(nodes.count(old_server));
  int term = current_term;
  std::string uuid;
  generate_special_uuid(uuid);

  std::set<std::string> prev_nodes(nodes), next_nodes(nodes);
  next_nodes.erase(old_server);
  int prev_index = last_conf_applied;
  int next_index = stored_log_num;
  std::string conf;
  build_conf_log(conf, prev_index, prev_nodes, next_index, next_nodes);
  set_log(next_index, term, uuid, conf);
  return next_index;
}