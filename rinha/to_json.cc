#ifndef NDEBUG
#define DEBUG 1
#else
#define DEBUG 0
#endif

#include "rinha/to_json.h"

#include <string>

#include "absl/strings/str_cat.h"
#include "glog/logging.h"

#include "rinha/structs.h"

namespace rinha {
namespace {
constexpr char kTotal[] = R"({"saldo":{"total":)";
constexpr char kData[] = R"(,"data_extrato":")";
constexpr char kLimite[] = R"(","limite":)";
constexpr char kUltimas[] = R"(},"ultimas_transacoes":[)";
constexpr char kTransacaoValor[] = R"({"valor":)";
constexpr char kTransacaoTipo[] = R"(,"tipo":")";
constexpr char kTransacaoDescricao[] = R"(","descricao":")";
constexpr char kTransacaoRealizada[] = R"(","realizada_em":")";
constexpr char kTransacaoEnd[] = R"("})";
constexpr char kEnd[] = R"(]})";

} // namespace

std::string to_json(const Transaction &transaction) {
  return absl::StrCat(
      kTransacaoValor,
      transaction.value > 0 ? transaction.value : -transaction.value,
      kTransacaoTipo, transaction.value > 0 ? "c" : "d", kTransacaoDescricao,
      transaction.description, kTransacaoRealizada, transaction.timestamp,
      kTransacaoEnd);
}

std::string to_json(const Transaction transactions[], int transaction_count,
                    int next_transaction_index) {
  std::string json = "";
  if (transaction_count < 10) {
    for (int i = next_transaction_index - 1; i >= 0; i--) {
      absl::StrAppend(&json, to_json(transactions[i]));
      if (i != 0) {
        absl::StrAppend(&json, ",");
      }
    }
  } else {
    int i = (next_transaction_index - 1 + 10) % 10;
    for (int j = 0; j < 10; j++) {
      absl::StrAppend(&json, to_json(transactions[i]));
      if (j != 9) {
        absl::StrAppend(&json, ",");
      }
      i = (i - 1 + 10) % 10;
    }
  }

  return json;
}

std::string CustomerToJson(const Customer &customer,
                           std::string &&data_extrato) {
  std::string json =
      absl::StrCat(kTotal, customer.balance, kData, std::move(data_extrato),
                   kLimite, customer.limit, kUltimas,
                   to_json(customer.transactions, customer.transaction_count,
                           customer.next_transaction_index),
                   kEnd);

  return json;
}

} // namespace rinha
