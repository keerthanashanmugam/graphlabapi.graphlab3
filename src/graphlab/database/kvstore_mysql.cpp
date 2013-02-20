/*
 * kvstore_mysql.cpp
 *
 *  Created on: Feb 7, 2013
 *      Author: svilen
 */

#include <graphlab/database/kvstore_mysql.hpp>
#include <graphlab/logger/assertions.hpp>

#include <mysql5/mysql/mysql.h>
#include <mysql5/mysql/mysqld_error.h>
#include <mysql5/mysql/my_global.h>
#include <mysql5/mysql/storage/ndb/ndb_types.h>

namespace graphlab {

const std::string mysql_db_name = "graphlab_db";
const std::string mysql_table_name = "graphlab_kv";
const std::string mysql_index_name = "graphlab_index";
const char* mysql_default_connstr = "localhost:1186";
const char* mysql_default_sock = "/opt/local/var/run/mysql5/mysqld.sock";
const char* mysql_default_addr = "localhost";
const char* mysql_default_user = "root";

const char* mysql_keyattr_name = "ID";
const char* mysql_valueattr_name = "VAL";

kvstore_mysql::kvstore_mysql() {
  ndb_init();
  MYSQL mysql;

  ASSERT_TRUE(mysql_init(&mysql));
  ASSERT_TRUE(mysql_real_connect(&mysql, mysql_default_addr, mysql_default_user, "", "", 0, mysql_default_sock, 0));

  mysql_query(&mysql, ("CREATE DATABASE "+mysql_db_name+";").c_str());
  ASSERT_FALSE(mysql_query(&mysql, ("USE "+mysql_db_name+";").c_str()));

  mysql_query(&mysql, ("CREATE TABLE "+mysql_table_name+" ("+mysql_keyattr_name+" INT UNSIGNED NOT NULL PRIMARY KEY, "+
                                                           mysql_valueattr_name+" BLOB NOT NULL)\n " +
                                                           "ENGINE = NDBCLUSTER PARTITION BY KEY ("+mysql_keyattr_name+");").c_str());

  mysql_query(&mysql, ("CREATE UNIQUE INDEX "+mysql_index_name+" ON "+mysql_table_name+"("+mysql_keyattr_name+");").c_str());

  Ndb_cluster_connection *cluster_connection = new Ndb_cluster_connection(mysql_default_connstr);
  ASSERT_TRUE(cluster_connection->connect(4, 5, 1) == 0);
  ASSERT_TRUE(cluster_connection->wait_until_ready(30, 0) == 0);

  _ndb = new Ndb(cluster_connection, mysql_db_name.c_str());
  ASSERT_FALSE(_ndb->init(1024));

  _dict = _ndb->getDictionary();
  ASSERT_TRUE(_dict != NULL);

  _index = _dict->getIndex(mysql_index_name.c_str(), mysql_table_name.c_str());
  ASSERT_TRUE(_index != NULL);

  _table = _dict->getTable(mysql_table_name.c_str());
  ASSERT_TRUE(_table != NULL);

  printf("MySQL connection open\n");
}

kvstore_mysql::~kvstore_mysql() {
  delete _ndb;
  _ndb = NULL;
  ndb_end(0);
  printf("MySQL connection closed\n");
}

#define PRINT_ERROR(code,msg) \
  std::cout << "Error in " << __FILE__ << ", line: " << __LINE__ \
            << ", code: " << code \
            << ", msg: " << msg << "." << std::endl

#define APIERROR(error) { \
  PRINT_ERROR(error.code,error.message); }

void kvstore_mysql::set(const key_type key, const value_type &value) {
  NdbTransaction *trans = _ndb->startTransaction();
  if (trans == NULL) APIERROR(_ndb->getNdbError());
  ASSERT_TRUE(trans != NULL);

//  NdbIndexOperation *op = trans->getNdbIndexOperation(mysql_index_name.c_str(), mysql_table_name.c_str());
  NdbOperation *op = trans->getNdbOperation(_table);
  if (op == NULL) APIERROR(_dict->getNdbError());
  ASSERT_TRUE(op != NULL);

  op->writeTuple();
  op->equal(mysql_keyattr_name, (int) key);
  NdbBlob *blob_handle = op->getBlobHandle(mysql_valueattr_name);
  blob_handle->setValue((void *) value.data(), (Uint32) value.length());

  int trans_result = trans->execute(NdbTransaction::Commit);
  ASSERT_FALSE(trans_result == -1);

  _ndb->closeTransaction(trans);
}

bool kvstore_mysql::get(const key_type key, value_type &value) {
  NdbTransaction *trans = _ndb->startTransaction();
  ASSERT_TRUE(trans != NULL);

  NdbIndexOperation *op = trans->getNdbIndexOperation(mysql_index_name.c_str(), mysql_table_name.c_str());
//  NdbOperation *op = trans->getNdbOperation(_table);
  ASSERT_TRUE(op != NULL);

  op->readTuple();
  op->equal(mysql_keyattr_name, (int) key);
  NdbBlob *blob_handle = op->getBlobHandle(mysql_valueattr_name);

  Uint64 blob_size;
  void *blob_data = malloc(mysql_max_blob_size);

  blob_handle->getValue(blob_data, mysql_max_blob_size);

  int trans_result = trans->execute(NdbTransaction::NoCommit);
  ASSERT_FALSE(trans_result == -1);

  bool found = (trans->getNdbError().classification == NdbError::NoError);
//  printf("Found: %d\n", found);
  if (found) {
    blob_handle->getLength(blob_size);
//    printf("Blob size: %lld\n", blob_size);
    value = std::string((char *) blob_data, blob_size);
  }
  free(blob_data);

  _ndb->closeTransaction(trans);

  return found;
}

std::vector<value_type> kvstore_mysql::range_get(const key_type key_lo, const key_type key_hi) {
  std::vector<value_type> result;

  NdbTransaction *trans = _ndb->startTransaction();
  ASSERT_TRUE(trans != NULL);

  NdbIndexScanOperation *op = trans->getNdbIndexScanOperation(mysql_index_name.c_str(), mysql_table_name.c_str());
//  NdbScanOperation *op = trans->getNdbScanOperation(mysql_table_name.c_str());
  ASSERT_TRUE(op != NULL);

  op->readTuples();

  int key_lo_int = (int) key_lo, key_hi_int = (int) key_hi;
  op->setBound(mysql_keyattr_name, NdbIndexScanOperation::BoundLE, &key_lo_int, (Uint32) sizeof(key_lo_int));
  op->setBound(mysql_keyattr_name, NdbIndexScanOperation::BoundGE, &key_hi_int, (Uint32) sizeof(key_hi_int));

  NdbBlob *blob_handle = op->getBlobHandle(mysql_valueattr_name);
  ASSERT_TRUE(blob_handle != NULL);

  Uint64 blob_size;
  void *blob_data = malloc(mysql_max_blob_size);

  blob_handle->getValue(blob_data, mysql_max_blob_size);

  ASSERT_FALSE(trans->execute(NdbTransaction::NoCommit) == -1);

  int res;
  printf("Entering loop\n");
  for (;;) {
    res = op->nextResult(true);
    if (res != 0)
      break;

//    printf("Getting next tuple\n");
    blob_handle->getLength(blob_size);
//    printf("Size: %d\n", blob_size);
    result.push_back(std::string((char *) blob_data, blob_size));
    printf("%s\n", result[result.size()-1].c_str());
  }

  free(blob_data);

  printf("Got %d results\n", result.size());

  _ndb->closeTransaction(trans);

  return result;
}

std::pair<bool, value_type> kvstore_mysql::background_get_thread(const key_type key) {
  NdbTransaction *trans = _ndb->startTransaction();
  ASSERT_TRUE(trans != NULL);

  NdbOperation *op = trans->getNdbIndexOperation(_index);
  ASSERT_TRUE(op != NULL);

  op->readTuple();
  op->equal(mysql_keyattr_name, (int) key);
  std::string value(op->getValue(mysql_valueattr_name, NULL)->aRef());

  ASSERT_FALSE(trans->execute(NdbTransaction::NoCommit) == -1);

  bool found = (trans->getNdbError().classification == NdbError::NoError);
  _ndb->closeTransaction(trans);

  return std::pair<bool, value_type>(found, value);
}

void kvstore_mysql::remove_all() {

}

}
