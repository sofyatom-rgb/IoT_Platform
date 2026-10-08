#include <drogon/drogon.h>

#include <cstdlib>

using namespace drogon;

int main() {
  // Подключение к PostgreSQL
  app().addDbClient(orm::PostgresConfig{"127.0.0.1",
                                        5433,
                                        "iot_platform",
                                        "postgres",
                                        "postgres",
                                        4,
                                        "default",
                                        false,
                                        "",
                                        3.0,
                                        false,
                                        {}});

  // Endpoint для проверки соединения с БД
  app().registerHandler(
      "/db-check",
      [](const HttpRequestPtr &req,
         std::function<void(const HttpResponsePtr &)> &&callback) {
        auto db = app().getDbClient();
        if (!db) {
          Json::Value response;
          response["status"] = "error";
          response["message"] = "Database client is not initialized";
          callback(HttpResponse::newHttpJsonResponse(response));
          return;
        }

        db->execSqlAsync(
            "SELECT 1",
            [callback](const orm::Result &result) {
              Json::Value response;
              response["status"] = "ok";
              response["db"] = "connected";
              callback(HttpResponse::newHttpJsonResponse(response));
            },
            [callback](const orm::DrogonDbException &e) {
              Json::Value response;
              response["status"] = "error";
              response["message"] = e.base().what();
              callback(HttpResponse::newHttpJsonResponse(response));
            });
      },
      {Get});

  // Endpoint для получения телеметрии
  app().registerHandler(
      "/telemetry",

      [](const HttpRequestPtr &req,
         std::function<void(const HttpResponsePtr &)> &&callback) {
        // Получаем JSON из HTTP-запроса
        auto json = req->getJsonObject();

        // Проверяем, что JSON вообще пришёл
        if (!json) {
          Json::Value response;
          response["status"] = "error";
          response["message"] = "Invalid JSON";

          callback(HttpResponse::newHttpJsonResponse(response));

          return;
        }

        // Получаем данные из JSON
        int deviceId = (*json)["device_id"].asInt();
        double temperature = (*json)["temperature"].asDouble();

        // Получаем подключение к БД
        auto db = app().getDbClient();

        // Записываем данные в таблицу
        db->execSqlAsync(
            "INSERT INTO telemetry (device_id, temperature) "
            "VALUES ($1, $2)",

            // Если запись прошла успешно
            [callback](const orm::Result &result) {
              Json::Value response;
              response["status"] = "ok";

              callback(HttpResponse::newHttpJsonResponse(response));
            },

            // Если произошла ошибка
            [callback](const orm::DrogonDbException &e) {
              Json::Value response;
              response["status"] = "error";
              response["message"] = e.base().what();

              callback(HttpResponse::newHttpJsonResponse(response));
            },

            deviceId, temperature);
      },

      {Post});

  // Запускаем HTTP-сервер на порту из переменной PORT или по умолчанию 8080.
  int port = 8080;
  const char *portEnv = std::getenv("PORT");
  if (portEnv != nullptr && portEnv[0] != '\0') {
    char *end = nullptr;
    long envPort = std::strtol(portEnv, &end, 10);
    if (end != portEnv && envPort >= 1 && envPort <= 65535) {
      port = static_cast<int>(envPort);
    }
  }

  app().addListener("0.0.0.0", port).run();

  return 0;
}