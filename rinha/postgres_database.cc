#include "rinha/postgres_database.h"

#include <thread>

#include "absl/cleanup/cleanup.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "glog/logging.h"
#include "pqxx/pqxx"

#include "rinha/structs.h"
#include "rinha/to_json.h"
#include "rinha/shared_lock.h"

namespace rinha {
namespace {
constexpr char kInsertPreparedStmt[] = "insert_stmt";
constexpr char kSelectPreparedStmt[] = "select_stmt";
constexpr char kSelectForSharePreparedStmt[] = "select_for_share_stmt";

absl::Mutex customer_write_mutexs[5];

thread_local bool initialized = false;
thread_local pqxx::connection db;

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

Customer DeserializeCustomer(pqxx::bytes &&buffer) {
  Customer customer;
  customer = std::move(*reinterpret_cast<Customer *>(buffer.data()));
  return customer;
}

bool LazyInitializeDb() {
  try {
    DLOG(INFO) << "Opening database" << std::endl;
    db = pqxx::connection("postgres://rinha@localhost:5432/mydb");
  } catch (const pqxx::failure &e) {
    LOG(ERROR) << "Exception occurred trying to open the db." << e.what();
    return false;
  }

  return true;
}

bool LazyInitializeStatement() {
  try {
    pqxx::work W(db);

    db.prepare(kSelectForSharePreparedStmt,
               "SELECT data FROM Users WHERE id = $1 FOR UPDATE");
    db.prepare(kInsertPreparedStmt,
               "INSERT INTO Users (id, data) VALUES ($1, $2) ON CONFLICT (id) "
               "DO UPDATE SET data = excluded.data");

    db.prepare(kSelectPreparedStmt, "SELECT data FROM Users WHERE id = $1");

  } catch (const pqxx::failure &e) {
    LOG(ERROR) << "Exception occurred trying to open the db." << e.what();
    return false;
  } catch (...) {
    LOG(ERROR) << "Exception occurred trying to open the db.";
    return false;
  }

  return true;
}

bool InitializeDbAndStatements() {
  if (!LazyInitializeDb()) {
    LOG(ERROR) << "Failed to open database" << std::endl;
    return false;
  }

  if (!LazyInitializeStatement()) {
    LOG(ERROR) << "Failed to initialize statements" << std::endl;
    return false;
  }

  initialized = true;

  return true;
}

bool InsertCustomer(const Customer &customer, int id, pqxx::work &W) {
  if (!initialized) {
    bool success = InitializeDbAndStatements();
    if (!success) {
      LOG(ERROR) << "Failed initialization" << std::endl;
      return false;
    }
  }

  const std::uint8_t *bytes = reinterpret_cast<const std::uint8_t *>(&customer);
  W.exec_prepared(kInsertPreparedStmt, id,
                  pqxx::binary_cast(bytes, sizeof(customer)));

  return true;
}

bool ReadCustomerNonTransactional(const int id, Customer *customer) {
  if (!initialized) {
    bool success = InitializeDbAndStatements();
    if (!success) {
      LOG(ERROR) << "Failed to init db or statements" << std::endl;
      return false;
    }
  }

  pqxx::nontransaction N(db);
  pqxx::result R = N.exec_prepared(kSelectPreparedStmt, id);
  const pqxx::row row = R[0];
  // TODO: Are there any copies happening here? Can we do away with the
  // conversion?
  pqxx::bytes serialized_data = row[0].as<pqxx::bytes>();
  *customer = DeserializeCustomer(std::move(serialized_data));

  return true;
}
} // namespace

bool PostgresInitializeDb() {
  if (!InitializeDbAndStatements()) {
    LOG(ERROR) << "Failed to initialize db";
    return false;
  }
  initialized = true;
  for (int i = 0; i < 5; i++) {
    try {
      pqxx::work w(db);
      InsertCustomer(kInitialCustomers[i], i, w);
      w.commit();
    } catch (const pqxx::failure &e) {
      LOG(ERROR) << "Exception occurred trying to insert initial customers: "
                 << e.what();
      return false;
    } catch (...) {
      LOG(ERROR) << "Exception occurred trying to open the db.";
      return false;
    }
  }

  return true;
}

bool PostgresDbGetCustomer(int id, Customer *customer) {
  if (id > 5 || id < 1) {
    return false;
  }
  id--;

  if (!ReadCustomerNonTransactional(id, customer)) {
    LOG(ERROR) << "Failed to read customer" << std::endl;
    return false;
  }

  return true;
}

TransactionResult PostgresDbExecuteTransaction(int id,
                                               Transaction &&transaction,
                                               Customer *customer) {
  if (id > 5 || id < 1) {
    return TransactionResult::NOT_FOUND;
  }
  id--;

  if (!initialized) {
    bool success = InitializeDbAndStatements();
    if (!success) {
      LOG(ERROR) << "Failed to init db or statements" << std::endl;
      return TransactionResult::NOT_FOUND;
    }
  }

  customer_write_mutexs[id].Lock();
  absl::Cleanup mutex_unlocker = [&] { customer_write_mutexs[id].Unlock(); };
  GetSharedLock(id);
  absl::Cleanup zoo_unlocker = [&] { ReleaseSharedLock(id);};

  if (!ReadCustomerNonTransactional(id, customer)) {
    LOG(ERROR) << "Failed to read customer" << std::endl;
    return TransactionResult::NOT_FOUND;
  }

  if (customer->balance + transaction.value < -customer->limit) {
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

  pqxx::work W(db);
  if (!InsertCustomer(*customer, id, W)) {
    LOG(ERROR) << "Failed to insert customer" << std::endl;
    return TransactionResult::NOT_FOUND;
  }

  try {
    W.commit();
  } catch (...) {
    LOG(ERROR) << "Failed to commit transaction";
    return TransactionResult::NOT_FOUND;
  }

  return TransactionResult::SUCCESS;
}

} // namespace rinha
