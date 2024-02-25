#include "rinha/from_http.h"

#include <optional>

#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/string_view.h"
#include "glog/logging.h"
#include "simdjson.h"

#include "rinha/structs.h"

namespace rinha {
namespace {
constexpr char kGetVerb[] = "GET";

constexpr char kPostPathBegin[] = " /clientes/";
constexpr char kPostPathEnd[] = "/transacoes ";
constexpr char kPostBeginBody[] = "\r\n\r\n";

thread_local simdjson::ondemand::parser parser;

bool ParseJsonBody(absl::string_view body, Transaction *transaction) {
  simdjson::ondemand::document doc;

  // TODO: we probably don' need to call strlen here, pass in the size.
  auto error =
      parser.iterate(body.data(), strlen(body.data()), sizeof(body.data()))
          .get(doc);
  if (error) {
    DLOG(ERROR) << "Error parsing json: " << error << std::endl;
    return false;
  }

  simdjson::ondemand::object obj;
  error = doc.get_object().get(obj);
  if (error) {
    DLOG(ERROR) << "Error getting object: " << error << std::endl;
    return false;
  }

  uint64_t valor = 0;
  bool valor_found = false;
  char tipo = 0;
  std::string descricao = "";
  for (auto field : obj) {
    simdjson::ondemand::raw_json_string key;
    error = field.key().get(key);
    if (error) {
      DLOG(ERROR) << "Error getting key: " << error << std::endl;
      return false;
    }

    if (key == "valor") {
      error = field.value().get_uint64().get(valor);
      if (error) {
        DLOG(ERROR) << "Error getting valor: " << error << std::endl;
        return false;
      }
      valor_found = true;
    } else if (key == "descricao") {
      std::string_view tmp;
      error = field.value().get_string().get(tmp);
      if (error) {
        DLOG(ERROR) << "Error getting descricao: " << error << std::endl;
        return false;
      }

      // TODO: copia necessaria?
      descricao = std::string(tmp);
    } else if (key == "tipo") {
      std::string_view tmp;
      error = field.value().get_string().get(tmp);

      if (error || tmp.size() != 1) {
        DLOG(ERROR) << "Error getting tipo: " << error << std::endl;
        return false;
      }

      tipo = tmp[0];
    } else {
      DLOG(ERROR) << "Unknown field: " << field.key() << std::endl;
      return false;
    }
  }

  if (tipo == 0) {
    DLOG(ERROR) << "Tipo not found" << std::endl;
    return false;
  }

  if (descricao.empty()) {
    DLOG(ERROR) << "Descricao not found" << std::endl;
    return false;
  }

  if (!valor_found) {
    DLOG(ERROR) << "Valor not found" << std::endl;
    return false;
  }

  if (tipo != 'c' && tipo != 'd') {
    DLOG(ERROR) << "Invalid tipo: " << tipo << std::endl;
    return false;
  }

  if (descricao.size() > 10) {
    DLOG(ERROR) << "Descricao too long: " << descricao << std::endl;
    return false;
  }

  transaction->value = valor;
  if (tipo == 'd') {
    transaction->value *= -1;
  }

  std::strcpy(transaction->description, descricao.c_str());

  return true;
}

} // namespace

bool FromHttp(absl::string_view http, Request *request) {
  if (absl::StartsWith(http, kGetVerb)) {
    DLOG(INFO) << "GET request" << std::endl;
    bool success =
        absl::SimpleAtoi(absl::string_view(http.data() + 14, 1), &request->id);
    if (!success) {
      DLOG(ERROR) << "Failed to parse id" << std::endl;
      return false;
    }
    request->type = RequestType::BALANCE;

    return true;
  }

  bool success =
      absl::SimpleAtoi(absl::string_view(http.data() + 15, 1), &request->id);
  if (!success) {
    DLOG(ERROR) << "Failed to parse id" << std::endl;
    return false;
  }

  size_t begin_body_idx = http.find(kPostBeginBody);
  if (begin_body_idx == std::string::npos) {
    DLOG(ERROR) << "No begin post body found" << std::endl;
    return false;
  }

  absl::string_view json_body(http.data() + begin_body_idx + 4,
                              http.size() - (begin_body_idx + 4));
  DLOG(INFO) << "json_body: " << json_body << std::endl;

  if (!ParseJsonBody(json_body, &request->transaction)) {
    DLOG(ERROR) << "Failed to parse json body" << std::endl;
    return false;
  }

  request->type = RequestType::TRANSACTION;
  return true;
}
} // namespace rinha
