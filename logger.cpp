#include "logger.hpp"

#include <errno.h>
#include <json/json.h>
#include <lmdb.h>
#include <sys/stat.h>

#include <cassert>
#include <string>

lmdb_raft_logger::lmdb_raft_logger(std::string _id) : raft_logger(_id) {}

lmdb_raft_logger::~lmdb_raft_logger() {}

void lmdb_raft_logger::get_nodes_from_buf(std::string buf,
                                          std::vector<std::string> &nodes) {
  if (buf.empty()) { return; }
  std::string::size_type pos = 0, next;

  do {
    next = buf.find(",", pos);
    nodes.emplace_back(buf.substr(pos, next - pos));
    pos = next + 1;
  } while (next != std::string::npos);
}

void lmdb_raft_logger::get_buf_from_nodes(std::string &buf,
                                          std::vector<std::string> nodes) {
  buf = "";
  for (std::string node : nodes) {
    buf += node;
    buf += ",";
  }
  buf.pop_back();
}

void lmdb_raft_logger::init(std::string addrs) {
  MDB_txn *txn;
  MDB_dbi dbi;
  MDB_stat stat;
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

  err = mdb_dbi_open(txn, log_db, MDB_CREATE, &dbi);
  if (err) {
    mdb_txn_abort(txn);
    abort();
  }

  err = mdb_stat(txn, dbi, &stat);
  if (err) {
    mdb_txn_abort(txn);
    abort();
  }

  if (stat.ms_entries == 0) {
    // Start init log

    // check if include self
    {
      bool has_self = false;
      std::vector<std::string> nbuf;
      get_nodes_from_buf(addrs, nbuf);
      for (std::string node : nbuf) {
        if (node == id) has_self = true;
      }
      if (!has_self) {
        // include self
        printf("not include self addr in %s,please add.\n", addrs.c_str());
        abort();
      }
    }

    // set dummy log, this make implimentation easily
    // index:0 ,term: 0
    stored_log_num = 0;
    Json::Value root;
    root["term"] = 0;
    root["uuid"] = generate_uuid();
    root["key"] = "__cluster";
    root["value"] = addrs;
    Json::StreamWriterBuilder builder;
    std::string log_str = Json::writeString(builder, root);
    save_log_str(0, log_str, txn);
    // save to state DB
    MDB_dbi cdbi;
    MDB_val cluster_value;
    err = mdb_dbi_open(txn, state_db, MDB_CREATE, &cdbi);

    if (err) {
      mdb_txn_abort(txn);
      abort();
    }

    cluster_value.mv_size = sizeof(char) * (addrs.size() + 1);
    cluster_value.mv_data = (void *)addrs.c_str();

    err = mdb_put(txn, cdbi, &cluster_key, &cluster_value, 0);

    if (err) {
      mdb_txn_abort(txn);
      abort();
    }

  } else {
    printf("log (and clusters info) exists already, so %s is ignored..\n",
           addrs.c_str());
    stored_log_num = stat.ms_entries - 1;
  }

  err = mdb_txn_commit(txn);
  if (err) {
    mdb_txn_abort(txn);
    abort();
  }
}

void lmdb_raft_logger::bootstrap_state_from_log(
  int &current_term, std::string &voted_for, std::vector<std::string> &nodes) {
  MDB_txn *txn;
  MDB_dbi dbi;
  MDB_val current_term_value, voted_for_value, cluster_value;
  int err;

  err = mdb_txn_begin(env, NULL, 0, &txn);
  assert(err == 0);

  err = mdb_dbi_open(txn, state_db, MDB_CREATE, &dbi);
  if (err) {
    mdb_txn_abort(txn);
    abort();
  }

  err = mdb_get(txn, dbi, &current_term_key, &current_term_value);
  if (err) {
    switch (err) {
      case MDB_NOTFOUND:
        current_term = 0;
        break;
      default:
        mdb_txn_abort(txn);
        abort();
        break;
    }
  } else {
    current_term = *((int *)current_term_value.mv_data);
  }

  err = mdb_get(txn, dbi, &voted_for_key, &voted_for_value);
  if (err) {
    switch (err) {
      case MDB_NOTFOUND:
        voted_for = "";
        break;
      default:
        mdb_txn_abort(txn);
        abort();
        break;
    }
  } else {
    voted_for = std::string((char *)voted_for_value.mv_data);
  }

  err = mdb_get(txn, dbi, &cluster_key, &cluster_value);
  if (err) {
    mdb_txn_abort(txn);
    abort();
  }
  std::string buf = std::string((char *)cluster_value.mv_data);
  get_nodes_from_buf(buf, nodes);

  err = mdb_txn_commit(txn);
  if (err) {
    mdb_txn_abort(txn);
    abort();
  }

  printf("bootstrap: current_term is %d\n", current_term);
  printf("bootstrap: voted_for is %s\n", voted_for.c_str());
}

void lmdb_raft_logger::save_current_term(int current_term) {
  assert(0 <= current_term);
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

  current_term_value = {sizeof(int), &current_term};

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
}

void lmdb_raft_logger::save_voted_for(std::string voted_for) {
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
  voted_for_value.mv_size = sizeof(char) * (voted_for.size() + 1);
  voted_for_value.mv_data = (void *)voted_for.c_str();

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
}

void lmdb_raft_logger::save_log_str(int index, std::string log_str,
                                    MDB_txn *ptxn) {
  assert(0 <= index);
  assert(index <= stored_log_num + 1);
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

  assert(stored_log_num == (int)stat.ms_entries - 1);

  err = mdb_txn_commit(txn);
  if (err) {
    mdb_txn_abort(txn);
    abort();
  }
}

void lmdb_raft_logger::save_uuid(std::string uuid, MDB_txn *ptxn) {
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

int lmdb_raft_logger::get_uuid(std::string uuid, MDB_txn *ptxn) {
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
    return MDB_NOTFOUND;
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
  return 0;
}

std::string lmdb_raft_logger::get_log_str(int index) {
  assert(0 <= index);
  if (index > stored_log_num) {
    // not found
    return "";
  }
  MDB_txn *txn;
  MDB_dbi dbi;
  MDB_val get_log_key, get_log_value;
  char get_log_key_buf[11];
  int err;

  err = mdb_txn_begin(env, NULL, 0, &txn);
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

int lmdb_raft_logger::append_log(int term, std::string uuid, std::string key,
                                 std::string value) {
  assert(0 <= term);
  int index = stored_log_num + 1;
  save_log(index, term, uuid, key, value);
  return index;
}

void lmdb_raft_logger::save_log(int index, int term, std::string uuid,
                                std::string key, std::string value) {
  assert(0 <= index);
  assert(index <= stored_log_num + 1);
  if (index == stored_log_num + 1) {
    // append log
    stored_log_num++;
  }
  Json::Value root;
  root["term"] = term;
  root["uuid"] = uuid;
  root["key"] = key;
  root["value"] = value;
  Json::StreamWriterBuilder builder;
  std::string log_str = Json::writeString(builder, root);
  MDB_txn *txn;
  int err = mdb_txn_begin(env, NULL, 0, &txn);
  assert(err == 0);
  save_log_str(index, log_str, txn);
  save_uuid(uuid, txn);
  err = mdb_txn_commit(txn);
  if (err) {
    mdb_txn_abort(txn);
    abort();
  }
  printf("save log index: %d,term: %d, uuid: %s,key: %s,value:%s \n", index,
         term, uuid.substr(0, 8).c_str(), key.c_str(), value.c_str());
}

int lmdb_raft_logger::get_term(int index) {
  assert(0 <= index);
  assert(index <= stored_log_num);
  int term;
  std::string uuid, key, value;
  get_log(index, term, uuid, key, value);
  return term;
}

void lmdb_raft_logger::get_log(int index, int &term, std::string &uuid,
                               std::string &key, std::string &value) {
  assert(0 <= index);
  assert(index <= stored_log_num);
  Json::CharReaderBuilder builder;
  Json::Value root;
  JSONCPP_STRING err_str;
  const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());

  std::string str = get_log_str(index);
  int ok =
    reader->parse(str.c_str(), str.c_str() + str.length(), &root, &err_str);
  assert(ok);
  term = root["term"].asInt();
  uuid = root["uuid"].asString();
  key = root["key"].asString();
  value = root["value"].asString();
}

void lmdb_raft_logger::get_last_log(int &index, int &term) {
  index = stored_log_num;
  std::string uuid, key, value;
  get_log(index, term, uuid, key, value);
  assert(0 <= index);
  assert(0 <= term);
}

bool lmdb_raft_logger::match_log(int index, int term) {
  assert(0 <= index);
  if (stored_log_num < index) { return false; }
  assert(index <= stored_log_num);
  int t = get_term(index);
  return t == term;
}

bool lmdb_raft_logger::uuid_already_exists(std::string uuid) {
  int err = get_uuid(uuid);
  if (err == MDB_NOTFOUND) { return false; }
  return true;
}

std::string lmdb_raft_logger::generate_uuid() {
  uuid_t id;
  uuid_generate(id);
  char sid[UUID_LENGTH];
  uuid_unparse_lower(id, sid);
  return std::string(sid);
}