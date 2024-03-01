#ifndef RINHA_MARIA_DATABASE_CC
#define RINHA_MARIA_DATABASE_CC

#include "absl/cleanup/cleanup.h"
#include "absl/synchronization/mutex.h"
#include "glog/logging.h"
#include "mysql.h"

#include "rinha/structs.h"
#include "rinha/to_json.h"

namespace rinha {
namespace {
thread_local MYSQL *conn;
thread_local MYSQL_STMT *insert_stmt;
thread_local MYSQL_STMT *update_stmt;
thread_local MYSQL_STMT *select_stmt;
thread_local MYSQL_STMT *select_for_update_stmt;

absl::Mutex customer_write_mutexs[5];

constexpr char stmt_insert[] = "INSERT INTO Users (id, data) VALUES (?, ?) ON "
                               "DUPLICATE KEY UPDATE data = VALUES(data)";

constexpr char stmt_update[] =
    "UPDATE Users SET  data = ?, version = version + 1 WHERE id = ? AND "
    "version = ?";

constexpr char stmt_select[] = "SELECT data, version FROM Users WHERE id = ?";

constexpr char stmt_select_for_update[] =
    "SELECT data, version FROM Users WHERE id = ? FOR UPDATE";

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

bool UpdateCustomer(const Customer &customer, const int id, const int version) {
  // Data, id, version
  MYSQL_BIND bind[3];
  memset(bind, 0, sizeof(bind));

  // Data
  unsigned long size = sizeof(Customer);
  bind[0].buffer_type = MYSQL_TYPE_BLOB;
  bind[0].buffer =
      const_cast<char *>(reinterpret_cast<const char *>(&customer));
  bind[0].buffer_length = size;
  bind[0].is_null = 0;
  bind[0].length = &size;

  // ID
  bind[1].buffer_type = MYSQL_TYPE_LONG;
  bind[1].buffer = const_cast<char *>(reinterpret_cast<const char *>(&id));
  bind[1].is_null = 0;
  bind[1].length = 0;

  // Version
  bind[2].buffer_type = MYSQL_TYPE_LONG;
  bind[2].buffer = const_cast<char *>(reinterpret_cast<const char *>(&version));
  bind[2].is_null = 0;
  bind[2].length = 0;

  MYSQL_STMT *stmt = update_stmt;

  if (mysql_stmt_bind_param(stmt, bind)) {
    LOG(ERROR) << "mysql_stmt_bind_param() failed";
    LOG(ERROR) << " " << mysql_stmt_error(stmt);
    mysql_stmt_free_result(stmt);
    return false;
  }

  if (mysql_stmt_execute(stmt)) {
    LOG(ERROR) << "mysql_stmt_execute(), 1 failed";
    LOG(ERROR) << " " << mysql_stmt_error(stmt);
    mysql_stmt_free_result(stmt);
    return false;
  }

  my_ulonglong affected_rows = mysql_affected_rows(conn);
  if (affected_rows == 0) {
    DLOG(ERROR) << "Lock conflict rerunning";
    mysql_stmt_free_result(stmt);
    return false;
  }

  mysql_stmt_free_result(stmt);
  return true;
}

bool InsertCustomer(const Customer &customer, const int id) {
  MYSQL_BIND bind[2];
  memset(bind, 0, sizeof(bind));

  // ID
  bind[0].buffer_type = MYSQL_TYPE_LONG;
  bind[0].buffer = const_cast<char *>(reinterpret_cast<const char *>(&id));
  bind[0].is_null = 0;
  bind[0].length = 0;

  unsigned long size = sizeof(Customer);
  bind[1].buffer_type = MYSQL_TYPE_BLOB;
  bind[1].buffer =
      const_cast<char *>(reinterpret_cast<const char *>(&customer));
  bind[1].buffer_length = size;
  bind[1].is_null = 0;
  bind[1].length = &size;

  if (mysql_stmt_bind_param(insert_stmt, bind)) {
    LOG(ERROR) << "mysql_stmt_bind_param() failed";
    LOG(ERROR) << " " << mysql_stmt_error(insert_stmt);
    mysql_stmt_free_result(insert_stmt);
    return false;
  }

  if (mysql_stmt_execute(insert_stmt)) {
    LOG(ERROR) << "mysql_stmt_execute(), 1 failed";
    LOG(ERROR) << " " << mysql_stmt_error(insert_stmt);
    mysql_stmt_free_result(insert_stmt);
    return false;
  }

  mysql_stmt_free_result(insert_stmt);
  return true;
}

bool LazyInitializeDb() {
  conn = mysql_init(NULL);
  if (conn == NULL) {
    LOG(ERROR) << "mysql_init() failed";
    return false;
  }

  if (mysql_real_connect(conn, "localhost", "rinha", "mypassword", "mydb", 0,
                         "/var/run/mysqld/mysqld.sock", 0) == NULL) {
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
    return false;
  }

  select_stmt = mysql_stmt_init(conn);
  if (!select_stmt) {
    LOG(ERROR) << "mysql_stmt_init(), out of memory";
    return false;
  }

  if (mysql_stmt_prepare(select_stmt, stmt_select, strlen(stmt_select))) {
    LOG(ERROR) << "mysql_stmt_prepare(), SELECT failed";
    LOG(ERROR) << " " << mysql_stmt_error(select_stmt);
    return false;
  }

  select_for_update_stmt = mysql_stmt_init(conn);
  if (!select_for_update_stmt) {
    LOG(ERROR) << "mysql_stmt_init(), out of memory";
    return false;
  }

  if (mysql_stmt_prepare(select_for_update_stmt, stmt_select_for_update,
                         strlen(stmt_select_for_update))) {
    LOG(ERROR) << "mysql_stmt_prepare(), SELECT failed";
    LOG(ERROR) << " " << mysql_stmt_error(select_for_update_stmt);
    return false;
  }

  update_stmt = mysql_stmt_init(conn);
  if (!update_stmt) {
    LOG(ERROR) << "mysql_stmt_init(), out of memory";
    return false;
  }

  if (mysql_stmt_prepare(update_stmt, stmt_update, strlen(stmt_update))) {
    LOG(ERROR) << "mysql_stmt_prepare(), SELECT failed";
    LOG(ERROR) << " " << mysql_stmt_error(update_stmt);
    return false;
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

  MYSQL_BIND params_bind[1];
  memset(params_bind, 0, sizeof(params_bind));

  // ID
  params_bind[0].buffer_type = MYSQL_TYPE_LONG;
  params_bind[0].buffer =
      const_cast<char *>(reinterpret_cast<const char *>(&id));
  params_bind[0].is_null = 0;
  params_bind[0].length = 0;

  if (mysql_stmt_bind_param(select_stmt, params_bind)) {
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
  bool is_null[2];
  bool error[2];
  unsigned long length[2];

  MYSQL_BIND results_bind[2];
  memset(results_bind, 0, sizeof(results_bind));

  // data
  results_bind[0].buffer_type = MYSQL_TYPE_BLOB;
  results_bind[0].buffer = blob_buffer;
  results_bind[0].buffer_length = sizeof(blob_buffer);
  results_bind[0].is_null = &is_null[0];
  results_bind[0].length = &length[0];
  results_bind[0].error = &error[0];

  // version
  results_bind[1].buffer_type = MYSQL_TYPE_LONG;
  results_bind[1].buffer =
      const_cast<char *>(reinterpret_cast<const char *>(version));
  results_bind[1].buffer_length = sizeof(*version);
  results_bind[1].is_null = &is_null[1];
  results_bind[1].length = &length[1];
  results_bind[1].error = &error[1];

  if (mysql_stmt_bind_result(select_stmt, results_bind)) {
    LOG(ERROR) << "mysql_stmt_execute(), 1 failed";
    LOG(ERROR) << " " << mysql_stmt_error(select_stmt);
    mysql_stmt_free_result(select_stmt);
    return false;
  }

  int rc = mysql_stmt_fetch(select_stmt);
  if (rc != 0) {
    LOG(ERROR) << "mysql_stmt_fetch(), 1 failed: "
               << mysql_stmt_error(select_stmt) << " "
               << mysql_stmt_errno(select_stmt) << " " << rc;
    mysql_stmt_free_result(select_stmt);
    return false;
  }

  *customer = std::move(*reinterpret_cast<Customer *>(blob_buffer));
  mysql_stmt_free_result(select_stmt);

  return true;
}

bool ReadCustomerForUpdate(const int id, Customer *customer, int *version) {
  DLOG(INFO) << "Reading customer " << id;

  thread_local MYSQL_STMT *stmt = select_for_update_stmt;

  MYSQL_BIND params_bind[1];
  memset(params_bind, 0, sizeof(params_bind));

  // ID
  params_bind[0].buffer_type = MYSQL_TYPE_LONG;
  params_bind[0].buffer =
      const_cast<char *>(reinterpret_cast<const char *>(&id));
  params_bind[0].is_null = 0;
  params_bind[0].length = 0;

  if (mysql_stmt_bind_param(stmt, params_bind)) {
    LOG(ERROR) << "mysql_stmt_bind_param() failed";
    LOG(ERROR) << " " << mysql_stmt_error(stmt);
    return false;
  }

  if (mysql_stmt_execute(stmt)) {
    LOG(ERROR) << "mysql_stmt_execute(), 1 failed";
    LOG(ERROR) << " " << mysql_stmt_error(stmt);
    return false;
  }

  unsigned char blob_buffer[sizeof(
      Customer)]; // Adjust the buffer size according to your needs
  bool is_null[2];
  bool error[2];
  unsigned long length[2];

  MYSQL_BIND results_bind[2];
  memset(results_bind, 0, sizeof(results_bind));

  // data
  results_bind[0].buffer_type = MYSQL_TYPE_BLOB;
  results_bind[0].buffer = blob_buffer;
  results_bind[0].buffer_length = sizeof(blob_buffer);
  results_bind[0].is_null = &is_null[0];
  results_bind[0].length = &length[0];
  results_bind[0].error = &error[0];

  // version
  results_bind[1].buffer_type = MYSQL_TYPE_LONG;
  results_bind[1].buffer =
      const_cast<char *>(reinterpret_cast<const char *>(version));
  results_bind[1].buffer_length = sizeof(*version);
  results_bind[1].is_null = &is_null[1];
  results_bind[1].length = &length[1];
  results_bind[1].error = &error[1];

  if (mysql_stmt_bind_result(stmt, results_bind)) {
    LOG(ERROR) << "mysql_stmt_execute(), 1 failed";
    LOG(ERROR) << " " << mysql_stmt_error(stmt);
    mysql_stmt_free_result(stmt);
    return false;
  }

  int rc = mysql_stmt_fetch(stmt);
  if (rc != 0) {
    LOG(ERROR) << "mysql_stmt_fetch(), 1 failed: " << mysql_stmt_error(stmt)
               << " " << mysql_stmt_errno(stmt) << " " << rc;
    mysql_stmt_free_result(stmt);
    return false;
  }

  *customer = std::move(*reinterpret_cast<Customer *>(blob_buffer));
  mysql_stmt_free_result(stmt);

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
    sleep(10);
  }

  LOG(INFO) << "Connected to MariaDB";

  for (int i = 0; i < 5; i++) {
    if (!InsertCustomer(kInitialCustomers[i], i)) {
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
  customer_write_mutexs[id].Lock();
  while (!success) {
    int version;

    DLOG(INFO) << "Starting transaction for customer " << id;
    if (mysql_query(conn, "START TRANSACTION")) {
      LOG(ERROR) << "Failed to start transaction: " << mysql_error(conn);
      continue;
    }

    if (!ReadCustomerForUpdate(id, customer, &version)) {
      LOG(ERROR) << "Failed to read customer";
      mysql_rollback(conn);
      continue;
    }

    if (customer->balance + transaction.value < -customer->limit) {
      customer_write_mutexs[id].Unlock();
      mysql_rollback(conn);
      return TransactionResult::LIMIT_EXCEEDED;
    }

    customer->balance += transaction.value;
    customer->transactions[customer->next_transaction_index] =
        std::move(transaction);
    customer->next_transaction_index =
        (customer->next_transaction_index + 1) % 10;
    if (customer->transaction_count <= 10) {
      customer->transaction_count++;
    }

    if (!InsertCustomer(*customer, id)) {
      DLOG(ERROR) << "Failed to insert customer" << std::endl;
      mysql_rollback(conn);
      continue;
    }

    if (mysql_commit(conn)) {
      LOG(ERROR) << "Failed to commit transaction: " << mysql_error(conn);
      mysql_rollback(conn);
      continue;
    }
    customer_write_mutexs[id].Unlock();
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
