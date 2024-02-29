#include <cstring>

#include "glog/logging.h"
#include "mariadb/mysql.h"

#include "rinha/structs.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "mariadb/mysql.h"

my_bool rinha_business_logic_udf_init(UDF_INIT *initid, UDF_ARGS *args,
                                      char *message) {
  initid->ptr = new char[sizeof(rinha::Customer)];
  initid->max_length = sizeof(rinha::Customer);
  initid->maybe_null = 1;
  return false;
}

void rinha_business_logic_udf_deinit(UDF_INIT *initid) { delete[] initid->ptr; }

char *rinha_business_logic_udf(UDF_INIT *initid, UDF_ARGS *args, char *result,
                               unsigned long *length, char *is_null,
                               char *error) {
  // TODO: Can we avoid this copy?
  memcpy(initid->ptr, args->args[0], sizeof(rinha::Customer));
  rinha::Customer *customer = reinterpret_cast<rinha::Customer *>(initid->ptr);
  rinha::Transaction *transaction =
      reinterpret_cast<rinha::Transaction *>(args->args[1]);

  if (customer->balance + transaction->value < -customer->limit) {
    *length = 0;
    *is_null = 1;
    return result;
  }

  customer->balance += transaction->value;
  // TODO: Can we avoid this copy?
  customer->transactions[customer->next_transaction_index] = *transaction;
  customer->next_transaction_index =
      (customer->next_transaction_index + 1) % 10;
  if (customer->transaction_count <= 10) {
    customer->transaction_count++;
  }

  *length = sizeof(rinha::Customer);
  result = initid->ptr;
  *is_null = 0;
  return result;
}

#ifdef __cplusplus
}
#endif
