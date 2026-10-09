#include <drogon/drogon.h>

using namespace drogon;

int main() {
  // Подключение к PostgreSQL
  app().createDbClient("postgresql", "127.0.0.1", 5433, "iot_platform",
                       "postgres", "postgres", 4, "", "default", false, "",
                       3.0);

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

  // Запускаем HTTP-сервер
  app().addListener("0.0.0.0", 8080).run();

  return 0;
}