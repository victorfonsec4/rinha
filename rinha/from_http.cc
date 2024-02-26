#include "rinha/from_http.h"

#include <optional>

#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/string_view.h"
#include "glog/logging.h"

#include "rinha/structs.h"

namespace rinha {
namespace {
constexpr char kGetVerb[] = "GET";

constexpr char kPostBeginBody[] = "\r\n\r\n";

bool SubstringToInt(absl::string_view body, size_t start, size_t end, int *x) {
  *x = 0;
  for (size_t i = start; i < end; ++i) {
    if (body[i] < '0' || body[i] > '9') {
      DLOG(INFO) << "Invalid valor";
      return false;
    }
    *x = *x * 10 + (body[i] - '0');
  }

  return true;
}

bool ParseJsonBody(absl::string_view body, Transaction *transaction) {
  static constexpr unsigned int num_begin_idx = 10;
  size_t num_end_idx = body.find(',', num_begin_idx);
  size_t num_dot_idx = body.find('.', num_begin_idx);
  if (num_dot_idx < num_end_idx || num_end_idx == std::string::npos) {
    DLOG(INFO) << "Invalid valor";
    return false;
  }

  bool success =
      SubstringToInt(body, num_begin_idx, num_end_idx, &transaction->value);
  if (!success) {
    DLOG(INFO) << "Failed to parse valor";
    return false;
  }
  DLOG(INFO) << "valor: " << transaction->value;

  size_t tipo_idx = num_end_idx + 11;
  if (body[tipo_idx] != 'c' && body[tipo_idx] != 'd') {
    DLOG(INFO) << "Invalid tipo";
    return false;
  }
  DLOG(INFO) << "tipo: " << body[tipo_idx];

  if (body[tipo_idx] == 'd') {
    transaction->value *= -1;
  }

  size_t start_desc_idx = tipo_idx + 18;
  size_t end_desc_idx = body.find('"', start_desc_idx);
  if (end_desc_idx == std::string::npos) {
    DLOG(INFO) << "Invalid description";
    return false;
  }

  if (end_desc_idx + 2 != body.size() || body[end_desc_idx + 1] != '}') {
    DLOG(INFO) << "Invalid json";
    return false;
  }

  if (end_desc_idx - start_desc_idx > 10 || end_desc_idx - start_desc_idx < 1) {
    DLOG(INFO) << "Descricao too long";
    return false;
  }

  for (size_t i = start_desc_idx; i < end_desc_idx; ++i) {
    transaction->description[i - start_desc_idx] = body[i];
  }
  transaction->description[end_desc_idx - start_desc_idx] = '\0';
  DLOG(INFO) << "description: " << transaction->description;

  return true;
}

} // namespace

bool FromHttp(absl::string_view http, Request *request) {
  if (absl::StartsWith(http, kGetVerb)) {
    DLOG(INFO) << "GET request";
    bool success =
        absl::SimpleAtoi(absl::string_view(http.data() + 14, 1), &request->id);
    if (!success) {
      DLOG(ERROR) << "Failed to parse id";
      return false;
    }
    request->type = RequestType::BALANCE;

    return true;
  }

  bool success =
      absl::SimpleAtoi(absl::string_view(http.data() + 15, 1), &request->id);
  if (!success) {
    DLOG(ERROR) << "Failed to parse id";
    return false;
  }

  size_t begin_body_idx = http.find(kPostBeginBody);
  if (begin_body_idx == std::string::npos) {
    DLOG(ERROR) << "No begin post body found";
    return false;
  }

  absl::string_view json_body(http.data() + begin_body_idx + 4,
                              http.size() - (begin_body_idx + 4));
  DLOG(INFO) << "json_body: " << json_body;

  if (!ParseJsonBody(json_body, &request->transaction)) {
    DLOG(ERROR) << "Failed to parse json body";
    return false;
  }

  request->type = RequestType::TRANSACTION;
  return true;
}
} // namespace rinha
