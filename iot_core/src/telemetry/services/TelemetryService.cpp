#include "telemetry/services/TelemetryService.h"

#include <stdexcept>
#include <utility>

TelemetryService::TelemetryService(
    std::shared_ptr<ITelemetryRepository> repository)
    : repository_(std::move(repository)) {
  if (!repository_) throw std::invalid_argument("TelemetryService requires a repository");
}

TelemetryService::TelemetryService(
    std::shared_ptr<ITelemetryRepository> repository,
    double temperatureThreshold,
    std::shared_ptr<IDevicesService> devices,
    std::shared_ptr<IUsersService> users,
    std::shared_ptr<INotificationServiceClient> notificationClient)
    : repository_(std::move(repository)),
      temperatureThreshold_(temperatureThreshold),
      devices_(std::move(devices)),
      users_(std::move(users)),
      notificationClient_(std::move(notificationClient)) {
  if (!repository_ || !devices_ || !users_ || !notificationClient_) {
    throw std::invalid_argument("TelemetryService requires all dependencies");
  }
}

void TelemetryService::save(
    int deviceId, double temperature,
    std::function<void(const std::string &)> success,
    std::function<void(const std::string &)> error) const {
  if (!devices_ || !users_ || !notificationClient_) {
    error("TelemetryService notification dependencies are not configured");
    return;
  }

  const auto devices = devices_;
  const auto users = users_;
  const auto notificationClient = notificationClient_;
  const double threshold = temperatureThreshold_;

  repository_->save(
      deviceId, temperature,
      [deviceId, temperature, threshold, devices, users, notificationClient,
       success = std::move(success)]() mutable {
        if (temperature <= threshold) {
          success("not_required");
          return;
        }

        devices->getNotificationInfo(
            deviceId,
            [temperature, threshold, users, notificationClient,
             success](int ownerId, const std::string &deviceName) mutable {
              users->getTelegramChatId(
                  ownerId,
                  [temperature, threshold, deviceName, notificationClient,
                   success](const std::string &chatId) mutable {
                    if (chatId.empty()) {
                      success("no_chat_id");
                      return;
                    }
                    notificationClient->sendTemperatureAlert(
                        chatId, deviceName, temperature, threshold,
                        [success]() mutable { success("sent"); },
                        [success](const std::string &notificationError) mutable {
                          success("delivery_failed: " + notificationError);
                        });
                  },
                  [success](const std::string &lookupError) mutable {
                    success("user_lookup_failed: " + lookupError);
                  });
            },
            [success](const std::string &lookupError) mutable {
              success("device_lookup_failed: " + lookupError);
            });
      },
      std::move(error));
}

void TelemetryService::getByDeviceId(
    int deviceId, std::function<void(const std::string &)> success,
    std::function<void(const std::string &)> error) const {
  repository_->getByDeviceId(deviceId, std::move(success), std::move(error));
}

void TelemetryService::deleteByDeviceId(
    int deviceId, std::function<void()> success,
    std::function<void(const std::string &)> error) const {
  repository_->deleteByDeviceId(deviceId, std::move(success), std::move(error));
}
