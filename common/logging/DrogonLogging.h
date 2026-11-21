#pragma once

#include <drogon/drogon.h>

#include <filesystem>
#include <string>
#include <utility>

namespace iot::logging {

/// Настраивает Drogon logger и общий журнал входящих HTTP-запросов.
inline void configureDrogonLogging(const std::filesystem::path &logDirectory,
                                   std::string serviceName) {
  std::filesystem::create_directories(logDirectory);

  drogon::app()
      .setLogPath(logDirectory.string(), serviceName, 10 * 1024 * 1024, 5)
      .setLogLevel(trantor::Logger::kInfo)
      .setLogLocalTime(true);

  // Логируем только метод и путь: query string, заголовки и тело могут
  // содержать пароли, токены и персональные данные.
  drogon::app().registerPreRoutingAdvice(
      [serviceName](const drogon::HttpRequestPtr &request) {
        LOG_INFO << '[' << serviceName << "] HTTP request: "
                 << request->methodString() << ' ' << request->path();
      });

  drogon::app().registerPostHandlingAdvice(
      [serviceName](const drogon::HttpRequestPtr &request,
                    const drogon::HttpResponsePtr &response) {
        if (!response) {
          LOG_ERROR << '[' << serviceName
                    << "] HTTP handler returned no response for "
                    << request->methodString() << ' ' << request->path();
          return;
        }

        const int status = static_cast<int>(response->getStatusCode());
        if (status >= 500) {
          LOG_ERROR << '[' << serviceName << "] HTTP response: "
                    << request->methodString() << ' ' << request->path()
                    << " -> " << status;
        } else if (status >= 400) {
          LOG_WARN << '[' << serviceName << "] HTTP response: "
                   << request->methodString() << ' ' << request->path()
                   << " -> " << status;
        } else {
          LOG_INFO << '[' << serviceName << "] HTTP response: "
                   << request->methodString() << ' ' << request->path()
                   << " -> " << status;
        }
      });
}

}  // namespace iot::logging
