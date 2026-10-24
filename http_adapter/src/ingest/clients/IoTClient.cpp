#include "ingest/clients/IoTClient.h"

#include <utility>

IoTClient::IoTClient(const std::string &baseUrl) : baseUrl_(baseUrl) {}

void IoTClient::sendTelemetry(
    int deviceId,
    double temperature,
    std::function<void()> success,
    std::function<void(const std::string &)> error) const {
  
  // Создаем HTTP клиент
  auto client = drogon::HttpClient::newHttpClient(baseUrl_);
  
  // Формируем запрос
  auto req = drogon::HttpRequest::newHttpRequest();
  req->setMethod(drogon::Post);
  req->setPath("/telemetry");
  
  // Формируем JSON тело
  Json::Value body;
  body["device_id"] = deviceId;
  body["temperature"] = temperature;
  req->setContentTypeCode(drogon::CT_APPLICATION_JSON);
  req->setBody(body.toStyledString()); // или Json::FastWriter().write(body)
  // Отправляем асинхронный запрос
  client->sendRequest(
      req, 
      [success, error](drogon::ReqResult result, const drogon::HttpResponsePtr &response) {
        if (result == drogon::ReqResult::Ok && response->getStatusCode() == drogon::k200OK) {
          success();
        } else {
          std::string errMsg = "Failed to send to iot_core. HTTP Status: " + 
                               std::to_string(response ? static_cast<int>(response->getStatusCode()) : 0);
          error(errMsg);
        }
      });
}

void IoTClient::getTelemetry(
    int deviceId,
    std::function<void(const std::string &)> success,
    std::function<void(const std::string &)> error) const {
  // Создаем HTTP клиент
  auto client = drogon::HttpClient::newHttpClient(baseUrl_);

  // Формируем GET-запрос с query-параметром device_id
  auto req = drogon::HttpRequest::newHttpRequest();
  req->setMethod(drogon::Get);
  req->setPath("/telemetry?device_id=" + std::to_string(deviceId));

  // Отправляем асинхронный запрос
  client->sendRequest(
      req,
      [success, error](drogon::ReqResult result, const drogon::HttpResponsePtr &response) {
        if (result == drogon::ReqResult::Ok && response->getStatusCode() == drogon::k200OK) {
          success(std::string(response->getBody()));
        } else {
          std::string errMsg = "Failed to get telemetry from iot_core. HTTP Status: " +
                               std::to_string(response ? static_cast<int>(response->getStatusCode()) : 0);
          error(errMsg);
        }
      });
}