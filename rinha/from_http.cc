#include "rinha/from_http.h"

#include <optional>

#include "absl/strings/string_view.h"
#include "glog/logging.h"
#include "simdjson.h"

#include "rinha/structs.h"

namespace rinha {
namespace {
constexpr char kGetVerb[] = "GET";
constexpr char kGetPathBegin[] = " clientes/";
constexpr char kGetPathEnd[] = "/extrato ";

constexpr char kPostVerb[] = "POST";
constexpr char kPostPathBegin[] = " clientes/";
constexpr char kPostPathEnd[] = "/transacoes ";
constexpr char kPostBeginBody[] = "\r\n\r\n";

// TODO: According to the docs only one document may be open at a time with
// a parser, when we start threading we need to account for this.
simdjson::ondemand::parser parser;

bool ParseJsonBody(absl::string_view body, Transaction *transaction) {
  // TODO: This copies the body again, maybe find a way to avoid this
  simdjson::padded_string p(body.data(), body.size());
  simdjson::ondemand::document doc;
  auto error = parser.iterate(p).get(doc);
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

  uint64_t valor;
  char tipo;
  std::string descricao;
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

  for (size_t i = 0; i < descricao.size(); i++) {
    transaction->description[i] = descricao[i];
  }
  transaction->description[descricao.size()] = '\0';

  return true;
}

} // namespace
Request from_http(absl::string_view http) {
  Request request;
  request.type = RequestType::INVALID;
  size_t first_end_line = http.find("\r\n");
  if (first_end_line == std::string::npos) {
    return request;
  }

  size_t get_verb_idx = http.find(kGetVerb);
  if (get_verb_idx != std::string::npos && get_verb_idx < first_end_line) {
    size_t get_path_idx = http.find(kGetPathBegin);
    if (get_path_idx == std::string::npos) {
      return request;
    }

    size_t id_idx = get_path_idx + sizeof(kGetPathBegin) - 1;
    size_t id_end_idx = http.find("/", id_idx);
    if (id_end_idx == std::string::npos) {
      return request;
    }

    size_t extrato_idx = http.find(kGetPathEnd);
    if (extrato_idx == std::string::npos) {
      return request;
    }

    request.id =
        std::stoi(std::string(http.substr(id_idx, id_end_idx - id_idx)));
    request.type = RequestType::BALANCE;

    return request;
  }

  size_t post_verb_idx = http.find(kPostVerb);
  if (post_verb_idx == std::string::npos || post_verb_idx >= first_end_line) {
    return request;
  }

  size_t post_path_idx = http.find(kPostPathBegin);
  if (post_path_idx == std::string::npos) {
    return request;
  }

  size_t id_idx = post_path_idx + sizeof(kPostPathBegin) - 1;
  size_t id_end_idx = http.find("/", id_idx);
  if (id_end_idx == std::string::npos) {
    return request;
  }

  size_t transacoes_idx = http.find(kPostPathEnd);
  if (transacoes_idx == std::string::npos) {
    return request;
  }

  size_t begin_body_idx = http.find(kPostBeginBody);
  if (begin_body_idx == std::string::npos) {
    return request;
  }

  std::string json_body = std::string(http.substr(begin_body_idx + 4));
  DLOG(INFO) << "json_body: " << json_body << std::endl;

  if (!ParseJsonBody(json_body, &request.transaction)) {
    return request;
  }

  request.id = std::stoi(std::string(http.substr(id_idx, id_end_idx - id_idx)));
  request.type = RequestType::TRANSACTION;
  return request;
}
} // namespace rinha
