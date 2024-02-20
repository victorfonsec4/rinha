#include "rinha/sqlite_database.h"

#include "absl/strings/str_cat.h"
#include "glog/logging.h"
#include "sqlite3.h"

#include "rinha/structs.h"
#include "rinha/to_json.h"
#include <thread>

namespace rinha {
namespace {

// Create SQL statement
constexpr char kCreateTable[] = "CREATE TABLE IF NOT EXISTS Users("
                                "ID INT PRIMARY KEY     NOT NULL,"
                                "DATA         BLOB );";

constexpr char kInsertSql[] =
    "INSERT OR REPLACE INTO Users (ID, DATA) VALUES (?, ?);";

constexpr char kSelectSql[] = "SELECT DATA FROM Users WHERE ID = ?;";

constexpr char kDatabasePath[] = "rinha.db";

thread_local sqlite3 *dbs[5];
thread_local sqlite3_stmt *insert_prepared_stmts[5];
thread_local sqlite3_stmt *select_prepared_stmts[5];

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

bool LazyInitializeDb(int i) {
  int rc = sqlite3_open(absl::StrCat(kDatabasePath, i).data(), &dbs[i]);
  if (rc) {
    LOG(ERROR) << "Can't open database: " << sqlite3_errmsg(dbs[i])
               << std::endl;
    return false;
  }

  return true;
}

bool LazyInitializeStatement(int i) {
  sqlite3 *&db = dbs[i];
  if (sqlite3_prepare_v2(db, kInsertSql, -1, &insert_prepared_stmts[i], NULL) !=
      SQLITE_OK) {
    LOG(ERROR) << "Failed to prepare statement: " << sqlite3_errmsg(db)
               << std::endl;
    return false;
  }

  if (sqlite3_prepare_v2(db, kSelectSql, -1, &select_prepared_stmts[i], NULL) !=
      SQLITE_OK) {
    LOG(ERROR) << "Failed to prepare statement: " << sqlite3_errmsg(db)
               << std::endl;
    return false;
  }

  return true;
}

bool InitializeDbAndStatements() {
  for (int i = 0; i < 5; i++) {
    if (!LazyInitializeDb(i)) {
      LOG(ERROR) << "Failed to open database" << std::endl;
      return false;
    }

    if (!LazyInitializeStatement(i)) {
      LOG(ERROR) << "Failed to initialize statements" << std::endl;
      return false;
    }
  }
  return true;
}

Customer DeserializeCustomer(const std::vector<uint8_t> &buffer) {
  Customer customer;
  std::memcpy(&customer, buffer.data(), sizeof(Customer));
  return customer;
}

std::vector<uint8_t> SerializeCustomer(const Customer &customer) {
  std::vector<uint8_t> buffer(sizeof(Customer));
  std::memcpy(buffer.data(), &customer, sizeof(customer));
  return buffer;
}

bool InsertCustomer(const Customer &customer, int id) {
  sqlite3 *&db = dbs[id];
  if (db == nullptr) {
    int success = InitializeDbAndStatements();
    if (success != 0) {
      LOG(ERROR) << "Failed to open database" << std::endl;
      return false;
    }
  }
  sqlite3_stmt *&stmt = insert_prepared_stmts[id];

  if (sqlite3_bind_int(stmt, 1, id) != SQLITE_OK) {
    LOG(ERROR) << "Failed to bind ID: " << sqlite3_errmsg(db) << std::endl;
    sqlite3_reset(stmt);
    return false;
  }

  std::vector<uint8_t> serialized_data = SerializeCustomer(customer);

  if (sqlite3_bind_blob(stmt, 2, serialized_data.data(), serialized_data.size(),
                        SQLITE_STATIC) != SQLITE_OK) {
    LOG(ERROR) << "Failed to bind DATA: " << sqlite3_errmsg(db) << std::endl;
    sqlite3_reset(stmt);
    return false;
  }

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    DLOG(ERROR) << "Insert failed: " << sqlite3_errmsg(db) << std::endl;
    sqlite3_reset(stmt);
    return false;
  }

  sqlite3_reset(stmt);

  return true;
}

bool ReadCustomer(const int id, Customer *customer) {
  sqlite3 *&db = dbs[id];

  if (db == nullptr) {
    bool success = InitializeDbAndStatements();
    if (!success) {
      LOG(ERROR) << "Failed to init db or statements" << std::endl;
      return false;
    }
  }
  sqlite3_stmt *&stmt = select_prepared_stmts[id];

  if (sqlite3_bind_int(stmt, 1, id) != SQLITE_OK) {
    LOG(ERROR) << "Failed to bind ID: " << sqlite3_errmsg(db) << std::endl;
    sqlite3_reset(stmt);
    return false;
  }

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    // Read the blob data
    const void *blobData = sqlite3_column_blob(stmt, 0);
    int blobSize = sqlite3_column_bytes(stmt, 0);

    // TODO: do we need this copy?
    std::vector<uint8_t> serialized_data(
        static_cast<const uint8_t *>(blobData),
        static_cast<const uint8_t *>(blobData) + blobSize);

    *customer = DeserializeCustomer(serialized_data);
    sqlite3_reset(stmt);
    return true;
  }

  LOG(ERROR) << "Read failed: No data found for ID " << id << std::endl;
  sqlite3_reset(stmt);
  return false;
}

bool BeginTransaction(sqlite3 *db) {
  char *errMsg = nullptr;
  int rc = sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr,
                        &errMsg);
  if (rc != SQLITE_OK) {
    LOG(ERROR) << "Error starting transaction: " << rc << std::endl << errMsg << std::endl;
    sqlite3_free(errMsg);
    return false;
  }
  return true;
}

bool EndTransaction(sqlite3 *db) {
  char *errMsg = nullptr;
  int rc =
      sqlite3_exec(db, "COMMIT;", nullptr, nullptr, &errMsg);
  if (rc != SQLITE_OK) {
    LOG(ERROR) << "Error committing transaction: " <<  rc << std::endl << errMsg << std::endl;
    sqlite3_free(errMsg);
    return false;
  }
  return true;
}

void RollbackTransaction(sqlite3 *db) {
  char *errMsg = nullptr;
  if (sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
    LOG(ERROR) << "Error rolling back transaction: " << errMsg << std::endl;
    sqlite3_free(errMsg);
  }
}

} // namespace

bool InitializeDb() {

  for (int i = 0; i < 5; i++) {
    LazyInitializeDb(i);
    sqlite3 *&db = dbs[i];

    char *zErrMsg = 0;
    int rc = sqlite3_exec(db, kCreateTable, nullptr, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
      LOG(ERROR) << "SQL error when creating table: " << zErrMsg << std::endl;
      sqlite3_free(zErrMsg);
      return false;
    }

    rc = sqlite3_exec(db, "PRAGMA journal_mode=WAL;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
      LOG(ERROR) << "Failed to set journal mode" << std::endl;
      return false;
    }

    rc = sqlite3_exec(db, "PRAGMA cache_size = 10000;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
      LOG(ERROR) << "Failed to set cache size" << std::endl;
      return false;
    }

    rc = sqlite3_exec(db, "PRAGMA synchronous = OFF;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
      LOG(ERROR) << "Failed to set synchronous" << std::endl;
      return false;
    }

    rc = sqlite3_exec(db, "PRAGMA mmap_size = 268435456;", NULL, NULL,
                      NULL); // 256 MB
    if (rc != SQLITE_OK) {
      LOG(ERROR) << "Failed to set mmap size" << std::endl;
      return false;
    }

    DLOG(INFO) << "Table created successfully" << std::endl;
  }

  for (int i = 0; i < 5; i++) {
    LazyInitializeStatement(i);

    if (!InsertCustomer(kInitialCustomers[i], i)) {
      LOG(ERROR) << "Failed to insert initial customer" << std::endl;
      return false;
    }
  }

  return true;
}

bool DbGetCustomer(int id, Customer *customer) {
  if (id > 5 || id < 1) {
    return false;
  }
  id--;

  if (!ReadCustomer(id, customer)) {
    LOG(ERROR) << "Failed to read customer" << std::endl;
    return false;
  }

  return true;
}

TransactionResult DbExecuteTransaction(int id, Transaction &&transaction) {
  if (id > 5 || id < 1) {
    return TransactionResult::NOT_FOUND;
  }
  id--;

  sqlite3 *&db = dbs[id];
  if (db == nullptr) {
    bool success = InitializeDbAndStatements();
    if (!success) {
      LOG(ERROR) << "Failed to init db or statements" << std::endl;
      return TransactionResult::NOT_FOUND;
    }
  }

  Customer customer;

  int max_retry = 100;
  int milliseconds_between_retries = 1;
  int current_retry = 0;
  bool success = false;
  bool begin_transaction;
  while (current_retry < max_retry && !success) {
    current_retry++;
    if (current_retry > 1) {
      if (begin_transaction) {
        RollbackTransaction(db);
        begin_transaction = false;
      }
      DLOG(WARNING) << "Retrying transaction" << std::endl;
      std::this_thread::sleep_for(
          std::chrono::milliseconds(milliseconds_between_retries));
      milliseconds_between_retries = std::min(2*milliseconds_between_retries, 3000);
    }

    begin_transaction = false;
    if (!BeginTransaction(db)) {
      DLOG(WARNING) << "Failed to start transaction" << std::endl;
      continue;
    }
    begin_transaction = true;

    if (!ReadCustomer(id, &customer)) {
      DLOG(WARNING) << "Failed to read customer" << std::endl;
      continue;
    }

    if (customer.balance + transaction.value < -customer.limit) {
      RollbackTransaction(db);
      return TransactionResult::LIMIT_EXCEEDED;
    }

    customer.balance += transaction.value;
    customer.transactions[customer.next_transaction_index] =
        std::move(transaction);
    customer.next_transaction_index =
        (customer.next_transaction_index + 1) % 10;
    if (customer.transaction_count <= 10) {
      customer.transaction_count++;
    }
    if (!InsertCustomer(customer, id)) {
      DLOG(WARNING) << "Failed to insert customer" << std::endl;
      continue;
    }

    if (!EndTransaction(db)) {
      DLOG(WARNING) << "Failed to commit transaction" << std::endl;
      continue;
    }

    success = true;
  }

  if (!success) {
    LOG(ERROR) << "FAILED TRANSACTION";
    return TransactionResult::NOT_FOUND;
  }

  return TransactionResult::SUCCESS;
}
} // namespace rinha
