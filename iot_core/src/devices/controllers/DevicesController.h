#pragma once

#include <drogon/drogon.h>

#include <memory>

#include "devices/services/IDevicesService.h"

// HTTP-слой устройств: принимает JSON и отправляет в сервис.
class DevicesController {
 public:
  /// Создаёт контроллер с сервисом устройств.
  explicit DevicesController(std::shared_ptr<IDevicesService> service);

  /// Регистрирует HTTP-маршруты устройств.
  void registerRoutes();

  /// Создаёт устройство из JSON-тела запроса.
  void create(const drogon::HttpRequestPtr &request,
              std::function<void(const drogon::HttpResponsePtr &)> callback);

  /// Удаляет устройство по ID.
  void deleteDevice(const drogon::HttpRequestPtr &request,
                    std::function<void(const drogon::HttpResponsePtr &)> callback);

  /// Изменяет параметры существующего устройства.
  void updateDevice(const drogon::HttpRequestPtr &request,
                    std::function<void(const drogon::HttpResponsePtr &)> callback);

  /// Возвращает данные устройства по ID.
  void getDevice(const drogon::HttpRequestPtr &request,
                 std::function<void(const drogon::HttpResponsePtr &)> callback);

 private:
  std::shared_ptr<IDevicesService> service_;
};