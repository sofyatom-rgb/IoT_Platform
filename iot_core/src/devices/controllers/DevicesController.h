#pragma once

#include <drogon/drogon.h>

#include <memory>

#include "devices/services/IDevicesService.h"

// HTTP-слой устройств: принимает JSON и отправляет в сервис.
class DevicesController {
 public:
  explicit DevicesController(std::shared_ptr<IDevicesService> service);

  void registerRoutes();

  void create(const drogon::HttpRequestPtr &request,
              std::function<void(const drogon::HttpResponsePtr &)> callback);

  void deleteDevice(int deviceId,
                    std::function<void(const drogon::HttpResponsePtr &)> callback);

  void updateDevice(int deviceId, const drogon::HttpRequestPtr &request,
                    std::function<void(const drogon::HttpResponsePtr &)> callback);

  void getDevice(int deviceId,
                 std::function<void(const drogon::HttpResponsePtr &)> callback);

 private:
  std::shared_ptr<IDevicesService> service_;
};