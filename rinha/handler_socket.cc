#include "rinha/handler_socket.h"

#include "glog/logging.h"
#include "libhsclient/hstcpcli.hpp"

#include "rinha/structs.h"

namespace rinha {
namespace {

thread_local dena::hstcpcli_ptr read_ptr;
thread_local int read_ptr_id = 1;
thread_local dena::hstcpcli_ptr write_ptr;
thread_local int write_ptr_id = 2;
static std::atomic<int> id_count = 0;
} // namespace

bool InitializeHs(absl::string_view host) {
  dena::verbose_level = 0;

  // initialize read
  {
    dena::config read_conf;
    read_conf["host"] = host;
    read_conf["port"] = "10019";
    read_conf["timeout"] = "600000";
    read_conf["listen_backlog"] = "256";

    dena::socket_args read_s_args;
    read_s_args.set(read_conf);
    read_ptr = dena::hstcpcli_i::create(read_s_args);
    read_ptr_id = id_count.fetch_add(1, std::memory_order_seq_cst);

    read_ptr->request_buf_open_index(
        /*identifies the tcpcli handler*/ read_ptr_id, "mydb", "Users",
        "handlers_index", "id,data");

    if (read_ptr->request_send() != 0) {
      LOG(ERROR) << "request_send failed: " << read_ptr->get_error();
      DCHECK(false);
      return false;
    }

    size_t nflds = 0;
    read_ptr->response_recv(nflds);
    DLOG(INFO) << "nflds: " << nflds;
    int e = read_ptr->get_error_code();
    if (e >= 0) {
      if (e != 0) {
        std::string s = read_ptr->get_error();
        DLOG(ERROR) << "error opening handle: " << s;
        DLOG(ERROR) << "error code: " << e;
        DCHECK(false);
        return false;
      }
      read_ptr->response_buf_remove();
    }
  }

  // Initialize write
  {
    dena::config write_conf;
    write_conf["host"] = host;
    write_conf["port"] = "10020";
    write_conf["timeout"] = "600000";
    write_conf["listen_backlog"] = "256";

    dena::socket_args write_s_args;
    write_s_args.set(write_conf);
    write_ptr = dena::hstcpcli_i::create(write_s_args);
    write_ptr_id = id_count.fetch_add(1, std::memory_order_seq_cst);

    write_ptr->request_buf_open_index(
        /*identifies the tcpcli handler*/ write_ptr_id, "mydb", "Users",
        "handlers_index", "id,data");

    int rc = write_ptr->request_send();
    if (rc != 0) {
      LOG(ERROR) << "request_send failed: " << write_ptr->get_error()
                 << " rc: " << rc;
      DCHECK(false);
      return false;
    }

    size_t nflds = 0;
    write_ptr->response_recv(nflds);
    DLOG(INFO) << "nflds: " << nflds;
    int e = write_ptr->get_error_code();
    if (e >= 0) {
      if (e != 0) {
        std::string s = write_ptr->get_error();
        DLOG(ERROR) << "error opening handle: " << s;
        DLOG(ERROR) << "error code: " << e;
        DCHECK(false);
        return false;
      }
      write_ptr->response_buf_remove();
    }
  }

  return true;
}

bool ReadCustomerHs(int id, Customer *customer) {
  char id_buf[2];
  id_buf[0] = id + '0';
  id_buf[1] = '\0';

  char op[] = "=";
  dena::string_ref id_ref(id_buf, 1);
  dena::string_ref op_ref(op, 1);
  dena::string_ref keys_ref[1] = {id_ref};
  dena::string_ref empty_ref;
  read_ptr->request_buf_exec_generic(/*tcpcli handler id*/ read_ptr_id, op_ref,
                                     /*keys to find=*/keys_ref,
                                     /*size keys=*/1,
                                     /*limit=*/0, /*skip=*/0,
                                     /*mod_op*/ empty_ref,
                                     /*mvs=*/&empty_ref, /*mvs_len=*/0);

  if (read_ptr->request_send() != 0) {
    LOG(ERROR) << "request_send failed";
    DCHECK(false);
    return false;
  }
  if (read_ptr->get_error_code() != 0) {
    std::string s = read_ptr->get_error();
    DLOG(ERROR) << "error sending request: " << s;
    DCHECK(false);
    return false;
  }

  size_t nflds = 0;
  read_ptr->response_recv(nflds);
  DLOG(INFO) << "nflds: " << nflds;
  if (read_ptr->get_error_code() != 0) {
    std::string s = read_ptr->get_error();
    DLOG(ERROR) << "error getting num fields: " << s;
    DCHECK(false);
    return false;
  }

  const dena::string_ref *row = 0;

  while ((row = read_ptr->get_next_row()) != 0) {
    DLOG(INFO) << "Found a row: " << row;
    for (size_t i = 0; i < nflds; i++) {
      const dena::string_ref &v = row[i];

      if (v.begin() != 0) {
        // Its a value
        // DLOG(INFO) << "value: " << std::string(v.begin(), v.end());
        if (i == 1) {
          // Its the data field

          rinha::Customer *c = const_cast<rinha::Customer *>(
              reinterpret_cast<const rinha::Customer *>(v.begin()));
          *customer = std::move(*c);
          DLOG(INFO) << "Customer limit: " << customer->limit
                     << " balance:" << customer->balance;
        }
      } else {
        // Its null
      }
    }
  }

  read_ptr->response_buf_remove();

  return true;
}

bool WriteCustomerHs(int id, const Customer &customer) {
  char id_write[2];
  id_write[0] = id + '0';
  id_write[1] = '\0';

  char op_write[] = "=";
  char mod_op_update[] = "U";
  dena::string_ref id_write_ref(id_write, 1);
  dena::string_ref op_write_ref(op_write, 1);
  dena::string_ref mod_op_update_ref(mod_op_update, 1);
  dena::string_ref update_value_ref(reinterpret_cast<const char *>(&customer),
                                    sizeof(rinha::Customer));
  dena::string_ref keys_write_ref[1] = {id_write_ref};
  dena::string_ref update_values_ref[2] = {id_write_ref, update_value_ref};
  write_ptr->request_buf_exec_generic(/*tcpcli handler id*/ write_ptr_id,
                                      op_write_ref,
                                      /*keys to find=*/keys_write_ref,
                                      /*size keys=*/1,
                                      /*limit=*/0, /*skip=*/0,
                                      /*mod_op*/ mod_op_update_ref,
                                      /*mvs=*/update_values_ref,
                                      /*mvs_len=*/2);

  if (write_ptr->request_send() != 0) {
    LOG(ERROR) << "request_send failed";
    DCHECK(false);
    return false;
  }
  if (write_ptr->get_error_code() != 0) {
    std::string s = write_ptr->get_error();
    DLOG(ERROR) << "error sending request: " << s;
    return 1;
  }

  size_t nflds = 0;
  int e = write_ptr->response_recv(nflds);
  DCHECK(e == 0);
  DLOG(INFO) << "nflds: " << nflds;
  if (write_ptr->get_error_code() != 0) {
    std::string s = write_ptr->get_error();
    DLOG(ERROR) << "error getting num fields: " << s;
    return 1;
  }

  const dena::string_ref *row = 0;

  while ((row = write_ptr->get_next_row()) != 0) {
    DLOG(INFO) << "Found a row: " << row;
    for (size_t i = 0; i < nflds; i++) {
      const dena::string_ref &v = row[i];

      if (v.begin() != 0) {
        // Its a value
        // DLOG(INFO) << "value: " << std::string(v.begin(), v.end());
      } else {
        // Its null
      }
    }
  }

  if (e >= 0) {
    write_ptr->response_buf_remove();
  }

  return true;
}

} // namespace rinha
