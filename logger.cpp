#include "logger.hpp"
#include <string>
#include <sys/stat.h>
#include <errno.h>
#include <cassert>

raft_logger::raft_logger(tl::endpoint id) {
  MDB_txn *txn;
  MDB_dbi dbi;
  MDB_stat *stat;
  std::string path = id;
  int err;

  err = mkdir(path.c_str(),0664);
  assert(err==0||err==EEXIST);

  err = mdb_env_open(env,path.c_str(),0,0664);
  assert(err==0);

  err = mdb_txn_begin(env,NULL,0,&txn);
  assert(err==0);

  err = mdb_dbi_open(txn,log_db,MDB_CREATE,&dbi);
  if(err) {
    mdb_txn_abort(txn);
    abort();
  }

  err = mdb_stat(txn,dbi,stat);
  if(err) {
    mdb_txn_abort(txn);
    abort();
  }

  stored_log_num = stat->ms_entries;

  err = mdb_txn_commit(txn);
  if(err) {
    mdb_txn_abort(txn);
    abort();
  }
}

raft_logger::~raft_logger() {
  
}

void raft_logger::bootstrap_state_from_log(int &current_term,std::string &voted_for) {
  MDB_txn *txn;
  MDB_dbi dbi;
  MDB_val current_term_value,voted_for_value;
  int err;

  err = mdb_txn_begin(env,NULL,0,&txn);
  assert(err==0);

  err = mdb_dbi_open(txn,state_db,MDB_CREATE,&dbi);
  if(err) {
    mdb_txn_abort(txn);
    abort();
  }

  err = mdb_get(txn,dbi,&current_term_key,&current_term_value);
  if(err) {
    switch(err) {
      case MDB_NOTFOUND:
        current_term = 1;
        break;
      default:
        mdb_txn_abort(txn);
        abort();
        break;
    }
  } else {
    current_term = *((int *)current_term_value.mv_data);
  }

  err = mdb_get(txn,dbi,&voted_for_key,&voted_for_value);
  if(err) {
    switch(err) {
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

  err = mdb_txn_commit(txn);
  if(err) {
    mdb_txn_abort(txn);
    abort();
  }

  printf("bootstrap: current_term is %d\n",current_term);
  printf("bootstrap: voted_for is %s\n",voted_for.c_str());
}

void raft_logger::save_current_term(int current_term) {
  MDB_txn *txn;
  MDB_dbi dbi;
  MDB_val current_term_value;
  int err;

  err = mdb_txn_begin(env,NULL,0,&txn);
  assert(err==0);

  err = mdb_dbi_open(txn,state_db,MDB_CREATE,&dbi);
  if(err) {
    mdb_txn_abort(txn);
    abort();
  }

  current_term_value = { sizeof(int), &current_term };

  err = mdb_put(txn,dbi,&current_term_key,&current_term_value,0);
  if(err) {
    mdb_txn_abort(txn);
    abort();
  }

  err = mdb_txn_commit(txn);
  if(err) {
    mdb_txn_abort(txn);
    abort();
  }
}

void raft_logger::save_voted_for(std::string voted_for) {
  MDB_txn *txn;
  MDB_dbi dbi;
  MDB_val voted_for_value;
  int err;

  err = mdb_txn_begin(env,NULL,0,&txn);
  assert(err==0);

  err = mdb_dbi_open(txn,state_db,MDB_CREATE,&dbi);
  if(err) {
    mdb_txn_abort(txn);
    abort();
  }

  // voted_for.size() is not include '\0' so +1
  voted_for_value.mv_size = sizeof(char) * (voted_for.size()+1);
  voted_for_value.mv_data = (void *)voted_for.c_str();

  err = mdb_put(txn,dbi,&voted_for_key,&voted_for_value,0);
  if(err) {
    mdb_txn_abort(txn);
    abort();
  }

  err = mdb_txn_commit(txn);
  if(err) {
    mdb_txn_abort(txn);
    abort();
  }
}

void raft_logger::save_log(int index,std::string log_str) {
  assert(0<index);
  assert(index<=stored_log_num+1);
  MDB_txn *txn;
  MDB_stat *stat;
  MDB_dbi dbi;
  MDB_val save_log_key,save_log_value;
  char save_log_key_buf[11];
  int err;

  err = mdb_txn_begin(env,NULL,0,&txn);
  assert(err==0);

  err = mdb_dbi_open(txn,log_db,0,&dbi);
  if(err) {
    mdb_txn_abort(txn);
    abort();
  }

  save_log_key.mv_size = 11;
  sprintf(save_log_key_buf,"%010d",index);
  save_log_key.mv_data = save_log_key_buf;

  // log_str.size() is not include '\0' so +1
  save_log_value.mv_size = sizeof(char) * (log_str.size()+1);
  save_log_value.mv_data = (void *)log_str.c_str();

  err = mdb_put(txn,dbi,&save_log_key,&save_log_value,0);
  if(err) {
    mdb_txn_abort(txn);
    abort();
  }

  err = mdb_stat(txn,dbi,stat);
  if(err) {
    mdb_txn_abort(txn);
    abort();
  }

  stored_log_num = stat->ms_entries;

  err = mdb_txn_commit(txn);
  if(err) {
    mdb_txn_abort(txn);
    abort();
  }

}

std::string raft_logger::get_log(int index) {
  assert(0<index);
  if(index>stored_log_num) {
    // not found
    return "";
  }
  MDB_txn *txn;
  MDB_dbi dbi;
  MDB_val get_log_key,get_log_value;
  char get_log_key_buf[11];
  int err;

  err = mdb_txn_begin(env,NULL,0,&txn);
  assert(err==0);

  err = mdb_dbi_open(txn,log_db,0,&dbi);
  if(err) {
    mdb_txn_abort(txn);
    abort();
  }

  get_log_key.mv_size = 11;
  sprintf(get_log_key_buf,"%010d",index);
  get_log_key.mv_data = get_log_key_buf;

  err = mdb_get(txn,dbi,&get_log_key,&get_log_value);
  if(err) {
    mdb_txn_abort(txn);
    abort();
  }

  err = mdb_txn_commit(txn);
  if(err) {
    mdb_txn_abort(txn);
    abort();
  }

  return std::string((char *)get_log_value.mv_data);
}