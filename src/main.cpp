#include <drogon/drogon.h>

using namespace drogon;

int main() {
  // 1. Говорим программе, где лежит база
  //    (порт 5432, потому что так ты запустил Docker)
  app().createDbClient("postgresql",   // тип базы
                       "127.0.0.1",    // адрес: твой компьютер
                       5432,           // порт
                       "iot_platform", // имя базы
                       "postgres",     // логин
                       "postgres",     // пароль
                       4               // сколько соединений держать
  );

  // 2. Создаём адрес /db-check
  app().registerHandler(
      "/db-check",
      [](const HttpRequestPtr &req,
         std::function<void(const HttpResponsePtr &)> &&callback) {
        // берём подключение к базе
        auto db = app().getDbClient();

        // задаём базе вопрос "SELECT 1" (ты жива?)
        db->execSqlAsync(
            "SELECT 1 AS ok",

            // если база ответила:
            [callback](const orm::Result &result) {
              Json::Value json;
              json["db"] = "ok";
              callback(HttpResponse::newHttpJsonResponse(json));
            },

            // если ошибка:
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