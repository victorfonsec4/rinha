#ifndef RINHA_MARIA_DATABASE_CC
#define RINHA_MARIA_DATABASE_CC

#include "absl/cleanup/cleanup.h"
#include "absl/synchronization/mutex.h"
#include "glog/logging.h"
#include "mariadb/mysql.h"

#include "rinha/shared_count.h"
#include "rinha/shared_lock.h"
#include "rinha/structs.h"
#include "rinha/to_json.h"

namespace rinha {
namespace {
thread_local MYSQL *conn;
thread_local MYSQL_STMT *insert_stmt;
thread_local MYSQL_STMT *select_stmt;

absl::Mutex customer_write_mutexs[5];

constexpr char stmt_insert[] =
    "INSERT INTO Users (id, data, version) VALUES (?, ?, ?) ON "
    "DUPLICATE KEY UPDATE data = VALUES(data), version = VALUES(version)";
constexpr char stmt_select[] = "SELECT data, version FROM Users WHERE id = ?";

constexpr Customer kInitialCustomers[] = {
    {.limit = 100000,
     .balance = 0,
     .transactions = {},
     .transaction_count = 0,
     .next_transaction_index = 0},
    {.limit = 80000,
     .balance = 0,
     .transactions = {},
     .transaction_count = 0,
     .next_transaction_index = 0},
    {.limit = 1000000,
     .balance = 0,
     .transactions = {},
     .transaction_count = 0,
     .next_transaction_index = 0},
    {.limit = 10000000,
     .balance = 0,
     .transactions = {},
     .transaction_count = 0,
     .next_transaction_index = 0},
    {.limit = 500000,
     .balance = 0,
     .transactions = {},
     .transaction_count = 0,
     .next_transaction_index = 0},

};

bool InsertCustomer(const Customer &customer, const int version, const int id) {
  MYSQL_BIND bind[3];
  memset(bind, 0, sizeof(bind));

  // ID
  bind[0].buffer_type = MYSQL_TYPE_LONG;
  bind[0].buffer = const_cast<char *>(reinterpret_cast<const char *>(&id));
  bind[0].is_null = 0;
  bind[0].length = 0;

  // Buffer
  unsigned long size = sizeof(Customer);
  bind[1].buffer_type = MYSQL_TYPE_BLOB;
  bind[1].buffer =
      const_cast<char *>(reinterpret_cast<const char *>(&customer));
  bind[1].buffer_length = size;
  bind[1].is_null = 0;
  bind[1].length = &size;

  // Version
  bind[2].buffer_type = MYSQL_TYPE_LONG;
  bind[2].buffer = const_cast<char *>(reinterpret_cast<const char *>(&version));
  bind[2].is_null = 0;
  bind[2].length = 0;

  if (mysql_stmt_bind_param(insert_stmt, bind)) {
    LOG(ERROR) << "mysql_stmt_bind_param() failed";
    LOG(ERROR) << " " << mysql_stmt_error(insert_stmt);
    return false;
  }

  if (mysql_stmt_execute(insert_stmt)) {
    LOG(ERROR) << "mysql_stmt_execute(), 1 failed";
    LOG(ERROR) << " " << mysql_stmt_error(insert_stmt);
    return false;
  }

  mysql_stmt_reset(insert_stmt);

  return true;
}

bool LazyInitializeDb() {
  conn = mysql_init(NULL);
  if (conn == NULL) {
    LOG(ERROR) << "mysql_init() failed";
    return false;
  }

  if (mysql_real_connect(conn, "localhost", "rinha", "mypassword", "mydb", 0,
                         "/run/mysqld/mysqld.sock", 0) == NULL) {
    LOG(ERROR) << "mysql_real_connect() failed : " << mysql_error(conn);
    return false;
  }

  return true;
}

bool LazyInitializeStatements() {
  // Prepare SQL statement
  insert_stmt = mysql_stmt_init(conn);
  if (!insert_stmt) {
    LOG(ERROR) << "mysql_stmt_init(), out of memory";
    return false;
  }

  if (mysql_stmt_prepare(insert_stmt, stmt_insert, strlen(stmt_insert))) {
    LOG(ERROR) << "mysql_stmt_prepare(), INSERT failed";
    LOG(ERROR) << " " << mysql_stmt_error(insert_stmt);
    return 1;
  }

  select_stmt = mysql_stmt_init(conn);
  if (!select_stmt) {
    LOG(ERROR) << "mysql_stmt_init(), out of memory";
    return false;
  }

  if (mysql_stmt_prepare(select_stmt, stmt_select, strlen(stmt_select))) {
    LOG(ERROR) << "mysql_stmt_prepare(), SELECT failed";
    LOG(ERROR) << " " << mysql_stmt_error(select_stmt);
    return 1;
  }

  return true;
}

bool LazyInit() {
  if (!LazyInitializeDb()) {
    return false;
  }

  if (!LazyInitializeStatements()) {
    return false;
  }

  return true;
}

bool ReadCustomer(const int id, Customer *customer, int *version) {
  DLOG(INFO) << "Reading customer " << id;

  MYSQL_BIND param_bind[1];
  memset(param_bind, 0, sizeof(param_bind));

  // ID
  param_bind[0].buffer_type = MYSQL_TYPE_LONG;
  param_bind[0].buffer =
      const_cast<char *>(reinterpret_cast<const char *>(&id));
  param_bind[0].is_null = 0;
  param_bind[0].length = 0;

  if (mysql_stmt_bind_param(select_stmt, param_bind)) {
    LOG(ERROR) << "mysql_stmt_bind_param() failed";
    LOG(ERROR) << " " << mysql_stmt_error(select_stmt);
    return false;
  }

  if (mysql_stmt_execute(select_stmt)) {
    LOG(ERROR) << "mysql_stmt_execute(), 1 failed";
    LOG(ERROR) << " " << mysql_stmt_error(select_stmt);
    return false;
  }

  unsigned char blob_buffer[sizeof(
      Customer)]; // Adjust the buffer size according to your needs
  my_bool is_null[2];
  my_bool error[2];
  unsigned long length[2];

  MYSQL_BIND result_bind[2];
  memset(result_bind, 0, sizeof(result_bind));

  // Blob
  result_bind[0].buffer_type = MYSQL_TYPE_BLOB;
  result_bind[0].buffer = blob_buffer;
  result_bind[0].buffer_length = sizeof(blob_buffer);
  result_bind[0].is_null = &is_null[0];
  result_bind[0].length = &length[0];
  result_bind[0].error = &error[0];

  // Version
  result_bind[1].buffer_type = MYSQL_TYPE_LONG;
  result_bind[1].buffer = reinterpret_cast<char *>(version);
  result_bind[1].is_null = &is_null[1];
  result_bind[1].length = &length[1];
  result_bind[1].error = &error[1];

  if (mysql_stmt_bind_result(select_stmt, result_bind)) {
    LOG(ERROR) << "mysql_stmt_execute(), 1 failed";
    LOG(ERROR) << " " << mysql_stmt_error(select_stmt);
    return false;
  }

  int rc = mysql_stmt_fetch(select_stmt);
  if (rc != 0) {
    LOG(ERROR) << "mysql_stmt_fetch(), 1 failed: "
               << mysql_stmt_error(select_stmt) << " "
               << mysql_stmt_errno(select_stmt) << " " << rc;
    return false;
  }

  *customer = std::move(*reinterpret_cast<Customer *>(blob_buffer));

  mysql_stmt_reset(select_stmt);

  return true;
}

} // namespace

bool MariaInitializeDb() {
  LOG(INFO) << "Initializing MariaDB";
  bool success = false;

  while (!success) {
    success = LazyInit();
    if (!success) {
      LOG(ERROR) << "Retrying to connect to MariaDB...";
    }
    sleep(5);
  }

  LOG(INFO) << "Connected to MariaDB";

  for (int i = 0; i < 5; i++) {
    if (!InsertCustomer(kInitialCustomers[i], 0, i)) {
      LOG(ERROR) << "Failed to insert initial customers";
      return false;
    }
  }

  LOG(INFO) << "Initialized MariaDB";

  return true;
}

bool MariaDbGetCustomer(int id, Customer *customer) {
  if (id > 5 || id < 1) {
    return false;
  }
  id--;

  int version;
  if (!ReadCustomer(id, customer, &version)) {
    LOG(ERROR) << "Failed to read customer";
    return false;
  }

  return true;
}

TransactionResult MariaDbExecuteTransaction(int id, Transaction &&transaction,
                                            Customer *customer) {
  if (id > 5 || id < 1) {
    return TransactionResult::NOT_FOUND;
  }
  id--;

  bool success = false;

  while (!success) {
    int version;
    if (!ReadCustomer(id, customer, &version)) {
      LOG(ERROR) << "Failed to read customer";
      return TransactionResult::NOT_FOUND;
    }

    DLOG(INFO) << "Read customer at version " << version;

    if (customer->balance + transaction.value < -customer->limit) {
      if (version == GetSharedCount(id)) {
        return TransactionResult::LIMIT_EXCEEDED;
      }
      LOG(ERROR) << "Update conflict, retrying...";
      continue;
    }

    customer->balance += transaction.value;
    customer->transactions[customer->next_transaction_index] =
        std::move(transaction);
    customer->next_transaction_index =
        (customer->next_transaction_index + 1) % 10;
    if (customer->transaction_count <= 10) {
      customer->transaction_count++;
    }

    customer_write_mutexs[id].Lock();
    absl::Cleanup mutex_unlocker = [&] { customer_write_mutexs[id].Unlock(); };
    GetSharedLock(id);
    absl::Cleanup shared_unlocker = [&] { ReleaseSharedLock(id); };
    if (version != GetSharedCount(id)) {
      DLOG(INFO) << "Conflict!" << std::endl
                 << "Current version: " << version
                 << " Shared count: " << GetSharedCount(id);
      LOG(ERROR) << "Update conflict, retrying...";
      continue;
    }

    if (!InsertCustomer(*customer, version + 1, id)) {
      LOG(ERROR) << "Failed to insert customer" << std::endl;
      return TransactionResult::NOT_FOUND;
    }

    IncreaseSharedCount(id);
    success = true;
  }

  return TransactionResult::SUCCESS;
}

bool MariaInitializeThread() {
  bool success = false;
  while (!success) {
    success = LazyInit();
    if (!success) {
      LOG(ERROR) << "Retrying to connect to MariaDB...";
    }
    sleep(1);
  }

  return true;
}

} // namespace rinha

#endif
