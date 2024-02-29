#include <cstring>

#include "glog/logging.h"
#include "mariadb/mysql.h"

#include "rinha/structs.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "mariadb/mysql.h"

my_bool print_customer_udf_init(UDF_INIT *initid, UDF_ARGS *args,
                                char *message) {
  return false;
}

void print_customer_udf_deinit(UDF_INIT *initid) {}

char *print_customer_udf(UDF_INIT *initid, UDF_ARGS *args, char *result,
                         unsigned long *length, char *is_null, char *error) {
  // TODO: Can we avoid this copy?
  rinha::Customer *customer =
      reinterpret_cast<rinha::Customer *>(args->args[0]);
  snprintf(result, 100, "Balance: %d Limit: %d", customer->balance,
           customer->limit);

  *length = strlen(result);
  return result;
}

#ifdef __cplusplus
}
#endif
