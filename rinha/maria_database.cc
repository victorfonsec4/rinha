#ifndef RINHA_MARIA_DATABASE_CC
#define RINHA_MARIA_DATABASE_CC

#include "absl/cleanup/cleanup.h"
#include "absl/synchronization/mutex.h"
#include "glog/logging.h"
#include "mariadb/mysql.h"

#include "rinha/handler_socket.h"
#include "rinha/structs.h"
#include "rinha/to_json.h"

namespace rinha {
namespace {
thread_local MYSQL *conn;
thread_local MYSQL_STMT *insert_stmt;
thread_local MYSQL_STMT *update_stmt;
thread_local MYSQL_STMT *select_stmt;
thread_local MYSQL_STMT *select_for_update_stmt;
thread_local MYSQL_STMT *get_lock_stmt;
thread_local MYSQL_STMT *release_lock_stmt;

absl::Mutex customer_write_mutexs[5];

constexpr char stmt_insert[] = "INSERT INTO Users (id, data) VALUES (?, ?) ON "
                               "DUPLICATE KEY UPDATE data = VALUES(data)";

constexpr char stmt_update[] =
    "UPDATE Users SET  data = ?, version = version + 1 WHERE id = ? AND "
    "version = ?";

constexpr char stmt_select[] = "SELECT data, version FROM Users WHERE id = ?";

constexpr char stmt_select_for_update[] =
    "SELECT data, version FROM Users WHERE id = ? FOR UPDATE";

constexpr char stmt_get_lock[] = "SELECT GET_LOCK(?, 100000000)";

constexpr char stmt_release_lock[] = "DO RELEASE_LOCK(?)";

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

bool GetLock(int id) {
  DLOG(INFO) << "Getting lock " << id;

  MYSQL_BIND params_bind[1];
  memset(params_bind, 0, sizeof(params_bind));

  thread_local MYSQL_STMT *stmt = get_lock_stmt;

  char id_str[1];
  id_str[0] = id + '0';
  // ID
  params_bind[0].buffer_type = MYSQL_TYPE_STRING;
  params_bind[0].buffer = id_str;
  params_bind[0].is_null = 0;
  params_bind[0].buffer_length = 1;

  if (mysql_stmt_bind_param(stmt, params_bind)) {
    LOG(ERROR) << "mysql_stmt_bind_param() failed";
    LOG(ERROR) << " " << mysql_stmt_error(stmt);
    DCHECK(false);
    return false;
  }

  if (mysql_stmt_execute(stmt)) {
    LOG(ERROR) << "mysql_stmt_execute(), 1 failed";
    LOG(ERROR) << " " << mysql_stmt_error(stmt);
    DCHECK(false);
    return false;
  }

  MYSQL_BIND results_bind[1];
  memset(results_bind, 0, sizeof(results_bind));
  my_bool is_null[1];
  my_bool error[1];
  unsigned long length[1];

  int success = 0;

  // Success 1 ok 0 fail
  results_bind[0].buffer_type = MYSQL_TYPE_LONG;
  results_bind[0].buffer =
      const_cast<char *>(reinterpret_cast<const char *>(&success));
  results_bind[0].buffer_length = sizeof(success);
  results_bind[0].is_null = &is_null[1];
  results_bind[0].length = &length[1];
  results_bind[0].error = &error[1];

  if (mysql_stmt_bind_result(stmt, results_bind)) {
    LOG(ERROR) << "mysql_stmt_execute(), 1 failed";
    LOG(ERROR) << " " << mysql_stmt_error(stmt);
    mysql_stmt_free_result(stmt);
    DCHECK(false);
    return false;
  }

  int rc = mysql_stmt_fetch(stmt);
  if (rc != 0) {
    LOG(ERROR) << "mysql_stmt_fetch(), 1 failed: " << mysql_stmt_error(stmt)
               << " " << mysql_stmt_errno(stmt) << " " << rc;
    mysql_stmt_free_result(stmt);
    DCHECK(false);
    return false;
  }
  DLOG(INFO) << "Is success null: " << is_null[0];
  DLOG(INFO) << "Success: " << success;
  DLOG(INFO) << "Finished getting lock " << id;
  DCHECK(success == 1);
  mysql_stmt_free_result(stmt);

  return true;
}

bool ReleaseLock(int id) {
  DLOG(INFO) << "Releasing lock " << id;

  MYSQL_BIND params_bind[1];
  memset(params_bind, 0, sizeof(params_bind));

  thread_local MYSQL_STMT *stmt = release_lock_stmt;

  char id_str[1];
  id_str[0] = id + '0';
  // ID
  params_bind[0].buffer_type = MYSQL_TYPE_STRING;
  params_bind[0].buffer = id_str;
  params_bind[0].buffer_length = 1;
  params_bind[0].is_null = 0;
  params_bind[0].length = 0;

  if (mysql_stmt_bind_param(stmt, params_bind)) {
    LOG(ERROR) << "mysql_stmt_bind_param() failed";
    LOG(ERROR) << " " << mysql_stmt_error(stmt);
    DCHECK(false);
    return false;
  }

  if (mysql_stmt_execute(stmt)) {
    LOG(ERROR) << "mysql_stmt_execute(), 1 failed";
    LOG(ERROR) << " " << mysql_stmt_error(stmt);
    DCHECK(false);
    return false;
  }

  mysql_stmt_free_result(stmt);

  return true;
}

bool LazyInitializeDb() {
  conn = mysql_init(NULL);
  if (conn == NULL) {
    LOG(ERROR) << "mysql_init() failed";
    return false;
  }

  if (mysql_real_connect(conn, "localhost", "rinha", "mypassword", "mydb", 0,
                         "/tmp/mysql.sock", 0) == NULL) {
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

  get_lock_stmt = mysql_stmt_init(conn);
  if (!get_lock_stmt) {
    LOG(ERROR) << "mysql_stmt_init(), out of memory";
    return false;
  }

  if (mysql_stmt_prepare(get_lock_stmt, stmt_get_lock, strlen(stmt_get_lock))) {
    LOG(ERROR) << "mysql_stmt_prepare(), SELECT failed";
    LOG(ERROR) << " " << mysql_stmt_error(get_lock_stmt);
    return false;
  }

  release_lock_stmt = mysql_stmt_init(conn);
  if (!release_lock_stmt) {
    LOG(ERROR) << "mysql_stmt_init(), out of memory";
    return false;
  }

  if (mysql_stmt_prepare(release_lock_stmt, stmt_release_lock,
                         strlen(stmt_release_lock))) {
    LOG(ERROR) << "mysql_stmt_prepare(), SELECT failed";
    LOG(ERROR) << " " << mysql_stmt_error(release_lock_stmt);
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

  if (!ReadCustomerHs(id, customer)) {
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
  DCHECK(GetLock(id));
  while (!success) {
    DLOG(INFO) << "Starting transaction for customer " << id;

    if (!ReadCustomerHs(id, customer)) {
      LOG(ERROR) << "Failed to read customer";
      continue;
    }

    if (customer->balance + transaction.value < -customer->limit) {
      DCHECK(ReleaseLock(id));
      customer_write_mutexs[id].Unlock();
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

    if (!WriteCustomerHs(id, *customer)) {
      DLOG(ERROR) << "Failed to insert customer" << std::endl;
      continue;
    }

    DCHECK(ReleaseLock(id));
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
