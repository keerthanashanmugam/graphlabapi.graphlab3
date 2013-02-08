/*
 * kvstore_mongodb.cpp
 *
 *  Created on: Feb 4, 2013
 *      Author: svilen
 */

#ifndef GRAPHLAB_DATABASE_KVSTORE_MONGODB_HPP
#define GRAPHLAB_DATABASE_KVSTORE_MONGODB_HPP

#include <mongo/client/dbclient.h>
#include <graphlab/database/kvstore_base.hpp>

namespace graphlab {

const char* MONGODB_DEFAULT_ADDR = "127.0.0.1";
const int MONGODB_DEFAULT_PORT = 27017;
const char* MONGODB_DEFAULT_NAMESPACE = "graphlab";

class kvstore_mongodb: public kvstore_base {
public:
  kvstore_mongodb() {
    kvstore_mongodb(MONGODB_DEFAULT_ADDR, MONGODB_DEFAULT_PORT, MONGODB_DEFAULT_NAMESPACE);
  }

  kvstore_mongodb(std::string addr, int port, std::string ns);
  virtual ~kvstore_mongodb();

  virtual void set(const key_type key, const value_type &value);

  virtual bool get(const key_type key, value_type &value);
  virtual std::vector<value_type> range_get(const key_type key_lo, const key_type key_hi);

  std::pair<bool, value_type> background_get_thread(const key_type key);

  virtual void remove_all();

private:
  mongo::DBClientConnection _conn;
  std::string _ns;
};

}

#endif

