#include "logger.hpp"
#include <string>
#include <sys/stat.h>
#include <errno.h>
#include <cassert>
#include <json/json.h>

std::string generate_path(tl::endpoint id) {
  std::string id_addr = id;
  std::string slash = "//";
  int slash_pos = id_addr.find(slash);
  std::string path = "log-" + id_addr.substr(slash_pos+slash.length(),id_addr.length()-slash_pos-slash.length());
  return path;
}

raft_logger::raft_logger(tl::endpoint id) {
  MDB_txn *txn;
  MDB_dbi dbi;
  MDB_stat stat;
  std::string path = generate_path(id);
  int err;

  err = mkdir(path.c_str(),0755);
  assert(err==0||err==EEXIST);

  err = mdb_env_create(&env);
  assert(err==0);

  err = mdb_env_set_maxdbs(env,100);
  assert(err==0);

  err = mdb_env_open(env,path.c_str(),0,0755);
  assert(err==0);

  err = mdb_txn_begin(env,NULL,0,&txn);
  assert(err==0);

  err = mdb_dbi_open(txn,log_db,MDB_CREATE,&dbi);
  if(err) {
    mdb_txn_abort(txn);
    abort();
  }

  err = mdb_stat(txn,dbi,&stat);
  if(err) {
    mdb_txn_abort(txn);
    abort();
  }

  

  if(stat.ms_entries==0) {
    // set dummy log, this make implimentation easily
    // index:0 ,term: -1
    stored_log_num = 0;
    save_log_str(0,"{\"index\":0,\"term\":-1,\"key\":\"hello\",\"value\":\"world\"}",txn);
  } else {
    stored_log_num = stat.ms_entries-1;
  }

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

void raft_logger::save_log_str(int index,std::string log_str,MDB_txn *ptxn) {
  assert(0<=index);
  assert(index<=stored_log_num+1);
  MDB_txn *txn;
  MDB_stat stat;
  MDB_dbi dbi;
  MDB_val save_log_key,save_log_value;
  char save_log_key_buf[11];
  int err;

  err = mdb_txn_begin(env,ptxn,0,&txn);
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

  err = mdb_stat(txn,dbi,&stat);
  if(err) {
    mdb_txn_abort(txn);
    abort();
  }

  stored_log_num = stat.ms_entries-1;

  err = mdb_txn_commit(txn);
  if(err) {
    mdb_txn_abort(txn);
    abort();
  }

}

std::string raft_logger::get_log_str(int index) {
  assert(0<=index);
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

int raft_logger::append_log(int term,std::string key,std::string value) {
  Json::Value root;
  root["term"]=term;
  root["key"]=key;
  root["value"]=value;
  Json::StreamWriterBuilder builder;
  std::string log_str = Json::writeString(builder, root);
  int index = stored_log_num;
  save_log_str(index,log_str);
  return index;
}

int raft_logger::get_term(int index) {
  assert(0<=index);
  assert(index <= stored_log_num);
  int term;
  std::string key,value;
  get_log(index,term,key,value);
  return term;
}

void raft_logger::get_log(int index,int &term,std::string &key,std::string &value) {
  assert(0<=index);
  assert(index <= stored_log_num);
  Json::CharReaderBuilder builder;
  Json::Value root;
  JSONCPP_STRING err_str;
  const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());

  std::string str = get_log_str(index);
  int ok = reader->parse(str.c_str(),str.c_str() + str.length(),&root,&err_str);
  assert(ok);
  term = root["term"].asInt();
  key  = root["key"].asString();
  value= root["value"].asString();
}

void raft_logger::get_last_log(int &index,int &term) {
  index = stored_log_num;
  std::string key,value;
  get_log(index,term,key,value);
}

bool raft_logger::match_log(int index,int term) {
  int t = get_term(index);
  return t == term;
} 