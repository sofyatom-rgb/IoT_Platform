#include "telemetry/services/TelemetryService.h"

#include "devices/services/DevicesService.h"
#include "telemetry/clients/NotificationServiceClient.h"
#include "telemetry/repositories/TelemetryRepository.h"
#include "users/services/UsersService.h"

#include <utility>

TelemetryService::TelemetryService(std::shared_ptr<drogon::orm::DbClient> db)
    : TelemetryService(std::move(db), 0.0, NotificationServiceClient("")) {}

TelemetryService::TelemetryService(
    std::shared_ptr<drogon::orm::DbClient> db, double temperatureThreshold,
    NotificationServiceClient notificationClient)
    : TelemetryService(
          std::make_shared<TelemetryRepository>(db),
          std::make_shared<DevicesService>(db),
          std::make_shared<UsersService>(db),
          std::make_shared<NotificationServiceClient>(
              std::move(notificationClient)),
          temperatureThreshold) {}
