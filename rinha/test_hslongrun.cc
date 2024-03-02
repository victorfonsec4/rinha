#include "glog/logging.h"
#include "libhsclient/hstcpcli.hpp"

#include "rinha/structs.h"

int main() {
  // Read

  dena::config read_conf;
  read_conf["host"] = "127.0.0.1";
  read_conf["port"] = "10019";
  read_conf["timeout"] = "600";
  read_conf["listen_backlog"] = "256";
  dena::verbose_level = 100;

  dena::socket_args read_s_args;
  read_s_args.set(read_conf);
  dena::hstcpcli_ptr tcpcli_read_ptr = dena::hstcpcli_i::create(read_s_args);
  dena::hstcpcli_i *tcpcli_read = tcpcli_read_ptr.get();

  tcpcli_read->request_buf_open_index(/*identifies the tcpcli handler*/ 1,
                                      "mydb", "Users", "handlers_index",
                                      "id,data");

  if (tcpcli_read->request_send() != 0) {
    LOG(ERROR) << "request_send failed: " << tcpcli_read->get_error();
    return 1;
  }

  size_t nflds = 0;
  tcpcli_read->response_recv(nflds);
  DLOG(INFO) << "nflds: " << nflds;
  int e = tcpcli_read->get_error_code();
  if (e >= 0) {
    if (e != 0) {
      std::string s = tcpcli_read->get_error();
      DLOG(ERROR) << "error opening handle: " << s;
      DLOG(ERROR) << "error code: " << e;
      return 1;
    }
    tcpcli_read->response_buf_remove();
  }

  char id[] = "1";
  char op[] = "=";
  dena::string_ref id_ref(id, 1);
  dena::string_ref op_ref(op, 1);
  dena::string_ref keys_ref[1] = {id_ref};
  dena::string_ref empty_ref;
  tcpcli_read->request_buf_exec_generic(/*tcpcli handler id*/ 1, op_ref,
                                        /*keys to find=*/keys_ref,
                                        /*size keys=*/1,
                                        /*limit=*/0, /*skip=*/0,
                                        /*mod_op*/ empty_ref,
                                        /*mvs=*/&empty_ref, /*mvs_len=*/0);

  if (tcpcli_read->request_send() != 0) {
    LOG(ERROR) << "request_send failed";
    return 1;
  }
  if (tcpcli_read->get_error_code() != 0) {
    std::string s = tcpcli_read->get_error();
    DLOG(ERROR) << "error sending request: " << s;
    return 1;
  }

  nflds = 0;
  tcpcli_read->response_recv(nflds);
  DLOG(INFO) << "nflds: " << nflds;
  if (tcpcli_read->get_error_code() != 0) {
    std::string s = tcpcli_read->get_error();
    DLOG(ERROR) << "error getting num fields: " << s;
    return 1;
  }

  const dena::string_ref *row = 0;

  rinha::Customer customer;
  while ((row = tcpcli_read->get_next_row()) != 0) {
    DLOG(INFO) << "Found a row: " << row;
    for (size_t i = 0; i < nflds; i++) {
      const dena::string_ref &v = row[i];

      if (v.begin() != 0) {
        // Its a value
        DLOG(INFO) << "value: " << std::string(v.begin(), v.end());
        if (i == 1) {
          // Its the data field

          rinha::Customer *c = const_cast<rinha::Customer *>(
              reinterpret_cast<const rinha::Customer *>(v.begin()));
          customer = std::move(*c);
          DLOG(INFO) << "Customer limit: " << customer.limit
                     << " balance:" << customer.balance;
        }
      } else {
        // Its null
      }
    }
  }

  tcpcli_read->response_buf_remove();

  // Write

  dena::config write_conf;
  write_conf["host"] = "127.0.0.1";
  write_conf["port"] = "10020";
  write_conf["timeout"] = "600";
  write_conf["listen_backlog"] = "256";

  dena::socket_args write_s_args;
  write_s_args.set(write_conf);
  dena::hstcpcli_ptr tcpcli_write_ptr = dena::hstcpcli_i::create(write_s_args);
  dena::hstcpcli_i *tcpcli_write = tcpcli_write_ptr.get();

  tcpcli_write->request_buf_open_index(/*identifies the tcpcli handler*/ 2,
                                       "mydb", "Users", "handlers_index",
                                       "id,data");

  int rc = tcpcli_write->request_send();
  if (rc != 0) {
    LOG(ERROR) << "request_send failed: " << tcpcli_write->get_error()
               << " rc: " << rc;
    return 1;
  }

  nflds = 0;
  tcpcli_write->response_recv(nflds);
  DLOG(INFO) << "nflds: " << nflds;
  e = tcpcli_write->get_error_code();
  if (e >= 0) {
    if (e != 0) {
      std::string s = tcpcli_write->get_error();
      DLOG(ERROR) << "error opening handle: " << s;
      DLOG(ERROR) << "error code: " << e;
      return 1;
    }
    tcpcli_write->response_buf_remove();
  }

  customer.balance += 100;

  char id_write[] = "1";
  char op_write[] = "=";
  char mod_op_update[] = "U";
  dena::string_ref id_write_ref(id_write, 1);
  dena::string_ref op_write_ref(op_write, 1);
  dena::string_ref mod_op_update_ref(mod_op_update, 1);
  dena::string_ref update_value_ref(reinterpret_cast<const char *>(&customer),
                                    sizeof(rinha::Customer));
  dena::string_ref keys_write_ref[1] = {id_write_ref};
  dena::string_ref update_values_ref[2] = {id_write_ref, update_value_ref};
  tcpcli_write->request_buf_exec_generic(/*tcpcli handler id*/ 2, op_write_ref,
                                         /*keys to find=*/keys_write_ref,
                                         /*size keys=*/1,
                                         /*limit=*/0, /*skip=*/0,
                                         /*mod_op*/ mod_op_update_ref,
                                         /*mvs=*/update_values_ref,
                                         /*mvs_len=*/2);

  if (tcpcli_write->request_send() != 0) {
    LOG(ERROR) << "request_send failed";
    return 1;
  }
  if (tcpcli_write->get_error_code() != 0) {
    std::string s = tcpcli_write->get_error();
    DLOG(ERROR) << "error sending request: " << s;
    return 1;
  }

  nflds = 0;
  tcpcli_write->response_recv(nflds);
  DLOG(INFO) << "nflds: " << nflds;
  if (tcpcli_write->get_error_code() != 0) {
    std::string s = tcpcli_write->get_error();
    DLOG(ERROR) << "error getting num fields: " << s;
    return 1;
  }

  row = 0;

  while ((row = tcpcli_write->get_next_row()) != 0) {
    DLOG(INFO) << "Found a row: " << row;
    for (size_t i = 0; i < nflds; i++) {
      const dena::string_ref &v = row[i];

      if (v.begin() != 0) {
        // Its a value
        DLOG(INFO) << "value: " << std::string(v.begin(), v.end());
      } else {
        // Its null
      }
    }
  }

  if (e >= 0) {
    tcpcli_write->response_buf_remove();
  }

  return 0;
}
