#include "glog/logging.h"
#include "libhsclient/hstcpcli.hpp"

int main() {

  dena::config conf;
  conf["host"] = "127.0.0.1";
  conf["port"] = "10019";
  conf["timeout"] = "600";
  conf["listen_backlog"] = "256";
  dena::verbose_level = 100;

  dena::socket_args s_args;
  s_args.set(conf);
  dena::hstcpcli_ptr tcpcli_ptr = dena::hstcpcli_i::create(s_args);
  dena::hstcpcli_i *tcpcli = tcpcli_ptr.get();

  tcpcli->request_buf_open_index(/*identifies the tcpcli handler*/ 1, "mydb",
                                 "Users", "handlers_index", "id,data");

  if (tcpcli->request_send() != 0) {
    LOG(ERROR) << "request_send failed: " << tcpcli->get_error();
    return 1;
  }

  size_t nflds = 0;
  tcpcli->response_recv(nflds);
  DLOG(INFO) << "nflds: " << nflds;
  int e = tcpcli->get_error_code();
  if (e >= 0) {
    if (e != 0) {
      std::string s = tcpcli->get_error();
      DLOG(ERROR) << "error opening handle: " << s;
      DLOG(ERROR) << "error code: " << e;
      return 1;
    }
    tcpcli->response_buf_remove();
  }

  char id[] = "1";
  char op[] = "=";
  dena::string_ref id_ref(id, 1);
  dena::string_ref op_ref(op, 1);
  dena::string_ref keys_ref[1] = {id_ref};
  dena::string_ref empty_ref;
  tcpcli->request_buf_exec_generic(/*tcpcli handler id*/ 1, op_ref,
                                   /*keys to find=*/keys_ref, /*size keys=*/1,
                                   /*limit=*/0, /*skip=*/0,
                                   /*mod_op*/ empty_ref,
                                   /*mvs=*/&empty_ref, /*mvs_len=*/0);

  if (tcpcli->request_send() != 0) {
    LOG(ERROR) << "request_send failed";
    return 1;
  }
  if (tcpcli->get_error_code() != 0) {
    std::string s = tcpcli->get_error();
    DLOG(ERROR) << "error sending request: " << s;
    return 1;
  }

  nflds = 0;
  tcpcli->response_recv(nflds);
  DLOG(INFO) << "nflds: " << nflds;
  if (tcpcli->get_error_code() != 0) {
    std::string s = tcpcli->get_error();
    DLOG(ERROR) << "error getting num fields: " << s;
    return 1;
  }

  const dena::string_ref *row = 0;

  while ((row = tcpcli->get_next_row()) != 0) {
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
    tcpcli->response_buf_remove();
  }

  return 0;
}
