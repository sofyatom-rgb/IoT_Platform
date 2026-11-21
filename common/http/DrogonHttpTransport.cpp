#include "common/http/DrogonHttpTransport.h"

#include <drogon/drogon.h>
#include <json/json.h>

#include <sstream>
#include <string>
#include <type_traits>
#include <utility>

namespace iot::http {
namespace {

/// Преобразует скаляр из внутреннего формата в значение JsonCpp.
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

/// Преобразует простое значение JsonCpp во внутренний формат.
JsonScalar fromJsonValue(const Json::Value &value) {
  if (value.isNull()) return nullptr;
  if (value.isBool()) return value.asBool();
  if (value.isString()) return value.asString();
  if (value.isInt64()) return static_cast<std::int64_t>(value.asInt64());
  if (value.isNumeric()) return value.asDouble();
  return nullptr;
}

/// Переводит HTTP-метод проекта в соответствующий метод Drogon.
const char *methodName(Method method) {
  switch (method) {
    case Method::Get: return "GET";
    case Method::Post: return "POST";
    case Method::Put: return "PUT";
    case Method::Delete: return "DELETE";
  }
  return "UNKNOWN";
}

// Removes URL credentials and query parameters before a URL reaches the log.
std::string safeBaseUrl(std::string url) {
  const auto scheme = url.find("://");
  if (scheme != std::string::npos) {
    const auto authorityStart = scheme + 3;
    const auto authorityEnd = url.find_first_of("/?#", authorityStart);
    const auto end = authorityEnd == std::string::npos ? url.size() : authorityEnd;
    const auto at = url.rfind('@', end);
    if (at != std::string::npos && at >= authorityStart) {
      url.replace(authorityStart, at - authorityStart, "<redacted>");
    }
  }

  const auto queryOrFragment = url.find_first_of("?#");
  if (queryOrFragment != std::string::npos) {
    url.erase(queryOrFragment);
  }
  return url;
}

// Telegram puts its bot token in the URL path. Never write it to the log.
std::string safePath(const std::string &path) {
  const auto bot = path.find("/bot");
  const auto sendMessage = path.find("/sendMessage", bot);
  if (bot == std::string::npos || sendMessage == std::string::npos) {
    return path;
  }
  return path.substr(0, bot) + "/bot<redacted>" + path.substr(sendMessage);
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
    LOG_ERROR << "[http] Cannot send " << methodName(request.method)
              << ' ' << safePath(request.path) << ": base URL is empty";
    if (completion) completion(Result::NetworkError, std::nullopt);
    return;
  }

  const std::string logPath = safePath(request.path);
  const std::string logBaseUrl = safeBaseUrl(baseUrl_);
  LOG_INFO << "[http] -> " << methodName(request.method) << ' '
           << logBaseUrl << logPath;

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
      [completion = std::move(completion), method = methodName(request.method),
       logPath, logBaseUrl](
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

        const int status = response ? response->statusCode : 0;
        if (result == drogon::ReqResult::Ok && response && status < 400) {
          LOG_INFO << "[http] <- " << method << ' ' << logBaseUrl << logPath
                   << " status=" << status;
        } else {
          LOG_ERROR << "[http] <- " << method << ' ' << logBaseUrl << logPath
                    << " failed; status=" << status
                    << (result == drogon::ReqResult::Ok
                            ? (response ? " http_error" : " no_response")
                            : " network_error");
        }

        if (completion) {
          completion(result == drogon::ReqResult::Ok ? Result::Ok
                                                     : Result::NetworkError,
                     std::move(response));
        }
      });
}

}  // namespace iot::http
