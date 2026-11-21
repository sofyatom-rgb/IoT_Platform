#include "telemetry/services/TelemetryService.h"

#include <drogon/drogon.h>

#include <memory>
#include <utility>

#include "devices/services/DevicesService.h"
#include "users/services/UsersService.h"

namespace {
void logNotificationError(const std::string &error) {
  LOG_ERROR << "Could not send temperature notification: " << error;
}
}  // namespace

TelemetryService::TelemetryService(drogon::orm::DbClientPtr db)
    : db_(db),
      repository_(std::move(db)),
      temperatureThreshold_(0.0),
      notificationClient_("") {}

TelemetryService::TelemetryService(
    drogon::orm::DbClientPtr db,
    double temperatureThreshold,
    NotificationServiceClient notificationClient)
    : db_(db),
      repository_(std::move(db)),
      temperatureThreshold_(temperatureThreshold),
      notificationClient_(std::move(notificationClient)) {}

void TelemetryService::save(
    int deviceId,
    double temperature,
    std::function<void(const std::string &)> success,
    std::function<void(const std::string &)> error) const {
  const auto db = db_;
  const auto threshold = temperatureThreshold_;
  const auto notificationClient = notificationClient_;
  auto finishSuccess =
      std::make_shared<std::function<void(const std::string &)>>(std::move(success));
  repository_.save(
      deviceId, temperature,
      [db, deviceId, temperature, threshold, notificationClient,
       finishSuccess]() {
        LOG_INFO << "Saved telemetry for device " << deviceId
                 << " with temperature " << temperature
                 << " (notification threshold " << threshold << ")";
        if (temperature <= threshold) {
          (*finishSuccess)("not_required");
          return;
        }

        DevicesService devices(db);
        devices.getNotificationInfo(
            deviceId, [db, temperature, threshold, notificationClient,
                       finishSuccess](
                int ownerId, const std::string &deviceName) {
              LOG_INFO << "Temperature threshold exceeded for device; "
                          "resolved owner";
              UsersService users(db);
              users.getTelegramChatId(
                  ownerId, [temperature, threshold, notificationClient,
                            deviceName, finishSuccess](
                      const std::string &chatId) {
                    if (chatId.empty()) {
                      LOG_WARN << "Temperature threshold exceeded, but user "
                                  "has no Telegram chat ID";
                      (*finishSuccess)("no_chat_id");
                      return;
                    }

                    notificationClient.sendTemperatureAlert(
                        chatId, deviceName, temperature, threshold,
                        [finishSuccess]() {
                          LOG_INFO << "Temperature notification accepted by "
                                      "notification service";
                          (*finishSuccess)("sent");
                        },
                        [finishSuccess](const std::string &notificationError) {
                          logNotificationError(notificationError);
                          (*finishSuccess)("delivery_failed: " +
                                           notificationError);
                        });
                  },
                  [finishSuccess](const std::string &lookupError) {
                    logNotificationError(lookupError);
                    (*finishSuccess)("user_lookup_failed: " + lookupError);
                  });
            },
            [finishSuccess](const std::string &lookupError) {
              logNotificationError(lookupError);
              (*finishSuccess)("device_lookup_failed: " + lookupError);
            });
      },
      std::move(error));
}

void TelemetryService::getByDeviceId(
    int deviceId,
    std::function<void(const std::string &)> success,
    std::function<void(const std::string &)> error) const {
  repository_.getByDeviceId(deviceId, std::move(success), std::move(error));
}

void TelemetryService::deleteByDeviceId(
    int deviceId,
    std::function<void()> success,
    std::function<void(const std::string &)> error) const {
  repository_.deleteByDeviceId(deviceId, std::move(success), std::move(error));
}
