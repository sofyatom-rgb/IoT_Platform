#include "telemetry/services/TelemetryService.h"

#include <stdexcept>
#include <utility>

TelemetryService::TelemetryService(
    std::shared_ptr<ITelemetryRepository> repository)
    : repository_(std::move(repository)), temperatureThreshold_(0.0) {
  if (!repository_) {
    throw std::invalid_argument("TelemetryService requires a repository");
  }
}

TelemetryService::TelemetryService(
    std::shared_ptr<ITelemetryRepository> repository,
    std::shared_ptr<IDevicesService> devicesService,
    std::shared_ptr<IUsersService> usersService,
    std::shared_ptr<INotificationServiceClient> notificationClient,
    double temperatureThreshold)
    : repository_(std::move(repository)),
      devicesService_(std::move(devicesService)),
      usersService_(std::move(usersService)),
      notificationClient_(std::move(notificationClient)),
      temperatureThreshold_(temperatureThreshold) {
  if (!repository_ || !devicesService_ || !usersService_ || !notificationClient_) {
    throw std::invalid_argument("TelemetryService requires all dependencies");
  }
}

void TelemetryService::save(
    int deviceId, double temperature,
    std::function<void(const std::string &)> success,
    std::function<void(const std::string &)> error) const {
  if (!devicesService_ || !usersService_ || !notificationClient_) {
    if (error) error("TelemetryService notification dependencies are not configured");
    return;
  }

  const auto threshold = temperatureThreshold_;
  const auto devicesService = devicesService_;
  const auto usersService = usersService_;
  const auto notificationClient = notificationClient_;
  auto finishSuccess =
      std::make_shared<std::function<void(const std::string &)>>(std::move(success));

  repository_->save(
      deviceId, temperature,
      [deviceId, temperature, threshold, devicesService, usersService,
       notificationClient, finishSuccess]() {
        if (temperature <= threshold) {
          (*finishSuccess)("not_required");
          return;
        }

        devicesService->getNotificationInfo(
            deviceId,
            [temperature, threshold, usersService, notificationClient,
             finishSuccess](int ownerId, const std::string &deviceName) {
              usersService->getTelegramChatId(
                  ownerId,
                  [temperature, threshold, notificationClient, deviceName,
                   finishSuccess](const std::string &chatId) {
                    if (chatId.empty()) {
                      (*finishSuccess)("no_chat_id");
                      return;
                    }

                    notificationClient->sendTemperatureAlert(
                        chatId, deviceName, temperature, threshold,
                        [finishSuccess]() {
                          (*finishSuccess)("sent");
                        },
                        [finishSuccess](const std::string &notificationError) {
                          (*finishSuccess)("delivery_failed: " +
                                           notificationError);
                        });
                  },
                  [finishSuccess](const std::string &lookupError) {
                    (*finishSuccess)("user_lookup_failed: " + lookupError);
                  });
            },
            [finishSuccess](const std::string &lookupError) {
              (*finishSuccess)("device_lookup_failed: " + lookupError);
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
