#include <drogon/drogon.h>

using namespace drogon;

int main() {
  // 1. Подключаемся к базе (данные такие же, как в docker-compose.yml)
  app().createDbClient("postgresql",             // тип базы
                       "127.0.0.1",              // адрес: твой компьютер
                       5433,                     // порт
                       "iot_platform",           // имя базы
                       "postgres",               // логин
                       "postgres",               // пароль
                       4,                        // сколько соединений держать
                       "", "default", false, "", // эти 4 значения не трогаем
                       3.0 // ждать ответа базу максимум 3 секунды
  );

  // 2. Создаём адрес /db-check
  app().registerHandler(
      "/db-check",
      [](const HttpRequestPtr &req,
         std::function<void(const HttpResponsePtr &)> &&callback) {
        auto db = app().getDbClient();

        // спрашиваем у базы: "ты жива?"
        db->execSqlAsync(
            "SELECT 1",

            // база ответила -> пишем "ok"
            [callback](const orm::Result &result) {
              Json::Value json;
              json["db"] = "ok";
              callback(HttpResponse::newHttpJsonResponse(json));
            },

            // база не ответила -> пишем "error"
            [callback](const orm::DrogonDbException &e) {
              Json::Value json;
              json["db"] = "error";
              json["message"] = e.base().what();
              callback(HttpResponse::newHttpJsonResponse(json));
            });
      },
      {Get});

  // 3. Запускаем сервер на порту 8080
  app().addListener("0.0.0.0", 8080).run();
}