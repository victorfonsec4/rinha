#include <cstring>

#include "glog/logging.h"
#include "mariadb/mysql.h"

#include "rinha/structs.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "mariadb/mysql.h"

my_bool get_account_udf_init(UDF_INIT *initid, UDF_ARGS *args, char *message) {
  initid->ptr = new char[sizeof(rinha::CustomerAccount)];
  initid->max_length = sizeof(rinha::CustomerAccount);
  initid->maybe_null = 0;
  return false;
}

void get_account_udf_deinit(UDF_INIT *initid) { delete[] initid->ptr; }

char *get_account_udf(UDF_INIT *initid, UDF_ARGS *args, char *result,
                      unsigned long *length, char *is_null, char *error) {
  rinha::Customer *customer =
      reinterpret_cast<rinha::Customer *>(args->args[0]);
  rinha::CustomerAccount *acc =
      reinterpret_cast<rinha::CustomerAccount *>(initid->ptr);

  acc->balance = customer->balance;
  acc->limit = customer->limit;

  *length = sizeof(rinha::CustomerAccount);
  result = initid->ptr;
  *is_null = 0;
  return result;
}

#ifdef __cplusplus
}
#endif
