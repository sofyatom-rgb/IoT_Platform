#include "common/http/DrogonHttpTransport.h"

#include <drogon/drogon.h>
#include <json/json.h>

#include <sstream>
#include <type_traits>
#include <utility>

namespace iot::http {
namespace {

Json::Value toJsonValue(const JsonScalar &value) {
  return std::visit(
      [](const auto &item) -> Json::Value {
        using T = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<T, std::nullptr_t>) {
          return Json::Value(Json::nullValue);
        } else if constexpr (std::is_same_v<T, std::string>) {
          return Json::Value(item);
        } else if constexpr (std::is_same_v<T, bool>) {
          return Json::Value(item);
        } else if constexpr (std::is_same_v<T, std::int64_t>) {
          return Json::Value(static_cast<Json::Int64>(item));
        } else {
          return Json::Value(item);
        }
      },
      value);
}

JsonScalar fromJsonValue(const Json::Value &value) {
  if (value.isNull()) return nullptr;
  if (value.isBool()) return value.asBool();
  if (value.isString()) return value.asString();
  if (value.isInt64()) return static_cast<std::int64_t>(value.asInt64());
  if (value.isNumeric()) return value.asDouble();
  return nullptr;
}

drogon::HttpMethod toDrogonMethod(Method method) {
  switch (method) {
    case Method::Get: return drogon::Get;
    case Method::Post: return drogon::Post;
    case Method::Put: return drogon::Put;
    case Method::Delete: return drogon::Delete;
  }
  return drogon::Get;
}

}  // namespace

DrogonHttpTransport::DrogonHttpTransport(std::string baseUrl)
    : baseUrl_(std::move(baseUrl)) {}

void DrogonHttpTransport::send(const Request &request,
                               Completion completion) const {
  if (baseUrl_.empty()) {
    if (completion) completion(Result::NetworkError, std::nullopt);
    return;
  }

  auto client = drogon::HttpClient::newHttpClient(baseUrl_);
  auto drogonRequest = drogon::HttpRequest::newHttpRequest();
  drogonRequest->setMethod(toDrogonMethod(request.method));
  drogonRequest->setPath(request.path);

  if (!request.jsonBody.empty()) {
    Json::Value body(Json::objectValue);
    for (const auto &[name, value] : request.jsonBody) {
      body[name] = toJsonValue(value);
    }
    Json::StreamWriterBuilder writer;
    writer["indentation"] = "";
    drogonRequest->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    drogonRequest->setBody(Json::writeString(writer, body));
  }

  client->sendRequest(
      drogonRequest,
      [completion = std::move(completion)](
          drogon::ReqResult result,
          const drogon::HttpResponsePtr &drogonResponse) mutable {
        std::optional<Response> response;
        if (drogonResponse) {
          Response converted;
          converted.statusCode =
              static_cast<int>(drogonResponse->getStatusCode());
          converted.body = std::string(drogonResponse->getBody());

          Json::CharReaderBuilder reader;
          Json::Value parsed;
          std::string parseError;
          std::istringstream input(converted.body);
          if (Json::parseFromStream(reader, input, &parsed, &parseError) &&
              parsed.isObject()) {
            converted.jsonParsed = true;
            for (const auto &name : parsed.getMemberNames()) {
              converted.jsonBody.emplace(name, fromJsonValue(parsed[name]));
            }
          } else {
            converted.jsonParseError = std::move(parseError);
          }
          response = std::move(converted);
        }

        if (completion) {
          completion(result == drogon::ReqResult::Ok ? Result::Ok
                                                     : Result::NetworkError,
                     std::move(response));
        }
      });
}

}  // namespace iot::http
