#include <drogon/drogon.h>
#include<drogon/HttpAppFramework.h>
#include <drogon/orm/Exception.h>
using namespace drogon;
void handlePing(const HttpRequestPtr& req,
                std::function<void(const HttpResponsePtr&)> &&callback) {
    // 1. Создаем HTTP-клиент Drogon с базовым URL
    auto client = HttpClient::newHttpClient("http://localhost:8080");

    // 2. Создаем HTTP-запрос
    auto reqOut = HttpRequest::newHttpRequest();
    reqOut->setMethod(Post);                 // ВАЖНО: /telemetry принимает POST
    reqOut->setPath("/telemetry");
    

    std::string idStr   = req->getParameter("device_id");
    std::string tempStr = req->getParameter("temperature");
    int    device_id   = std::stoi(idStr);
    double temperature = std::stod(tempStr);
    // Тело запроса — то, что ждёт /telemetry
    Json::Value body;
    body["device_id"]   = device_id;
    body["temperature"] = temperature;
    
    // Устанавливаем тело запроса как сериализованный JSON и указываем Content-Type
    reqOut->setContentTypeCode(CT_APPLICATION_JSON); // Указываем, что тело - это JSON
    reqOut->setBody(Json::FastWriter().write(body)); // Сериализуем JSON в строку и устанавливаем 
    
    // 3. Отправляем запрос асинхронно
    client->sendRequest(
        reqOut,
        [callback](ReqResult result, const HttpResponsePtr& respOut) {
            // Этот код выполнится, когда придет ответ (или ошибка)
            auto resp = HttpResponse::newHttpResponse();

            if (result == ReqResult::Ok && respOut) {
                resp->setStatusCode(k200OK);
                resp->setBody(std::string(respOut->getBody()) + '\n');
            } else {
                resp->setStatusCode(k502BadGateway);
                resp->setBody("Upstream error\n");
            }

            // 4. Возвращаем ответ через callback
            callback(resp);
        }
    );
}
int main() {
    //Регистрируем обработчик для /ping
    drogon::app().registerHandler(
        "/ping",
        [](const drogon::HttpRequestPtr &req,
           std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setBody("pong\n");
            callback(resp);
        },
        {drogon::Get}
    );


    drogon::app().registerHandler(
        "/set_telemetry",
        &handlePing,
        {drogon::Get}
    );
    // Запускаем сервер на порту 8080
    LOG_INFO << "HTTP Adapter running on http://127.0.0.1:8081";
    drogon::app().addListener("0.0.0.0", 8081).run();
    return 0;
}