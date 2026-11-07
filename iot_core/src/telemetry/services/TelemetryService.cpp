#include "telemetry/services/TelemetryService.h"

#include <stdexcept>
#include <string>
#include <utility>

namespace {
std::string summarizeChannels(const NotificationChannelStatuses &channels) {
  const bool telegramSent = channels.telegram == "sent";
  const bool emailSent = channels.email == "sent";
  if ((telegramSent && (emailSent || channels.email == "skipped")) ||
      (emailSent && channels.telegram == "skipped")) {
    return "sent";
  }

  const bool onlyDuplicates =
      (channels.telegram == "skipped_duplicate" ||
       channels.email == "skipped_duplicate") &&
      (channels.telegram == "skipped" ||
       channels.telegram == "skipped_duplicate") &&
      (channels.email == "skipped" || channels.email == "skipped_duplicate");
  if (onlyDuplicates) return "duplicate_suppressed";
  if (telegramSent || emailSent) {
    std::string details;
    if (channels.telegram != "skipped") {
      details = "telegram=" + channels.telegram;
    }
    if (channels.email != "skipped") {
      if (!details.empty()) details += ", ";
      details += "email=" + channels.email;
    }
    return "sent_partial: " + details;
  }
  return "delivery_failed";
}
}  // namespace

TelemetryService::TelemetryService(
    std::shared_ptr<ITelemetryRepository> repository)
    : repository_(std::move(repository)) {
  if (!repository_) {
    throw std::invalid_argument("TelemetryService requires a repository");
  }
}

TelemetryService::TelemetryService(
    std::shared_ptr<ITelemetryRepository> repository,
    double temperatureThreshold, std::shared_ptr<IDevicesService> devices,
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
    if (error) error("TelemetryService notification dependencies are not configured");
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
          notificationClient->clearTemperatureAlert(
              deviceId,
              [success]() mutable {
                if (success) success("not_required");
              },
              [success](const std::string &resetError) mutable {
                if (success) {
                  success("not_required; alert reset failed: " + resetError);
                }
              });
          return;
        }

        devices->getNotificationInfo(
            deviceId,
            [deviceId, temperature, threshold, users, notificationClient,
             success](int ownerId, const std::string &deviceName) mutable {
              users->getNotificationTargets(
                  ownerId,
                  [deviceId, temperature, threshold, notificationClient,
                   deviceName, success](
                      const NotificationTargets &targets) mutable {
                    if (targets.chatId.empty() && targets.email.empty()) {
                      if (success) success("no_chat_id");
                      return;
                    }
                    notificationClient->sendTemperatureAlert(
                        deviceId, targets.chatId, targets.email, deviceName,
                        temperature, threshold,
                        [success](const NotificationChannelStatuses &channels) mutable {
                          if (success) success(summarizeChannels(channels));
                        },
                        [success](const std::string &notificationError) mutable {
                          if (success) {
                            success("delivery_failed: " + notificationError);
                          }
                        });
                  },
                  [success](const std::string &lookupError) mutable {
                    if (success) {
                      success("user_lookup_failed: " + lookupError);
                    }
                  });
            },
            [success](const std::string &lookupError) mutable {
              if (success) {
                success("device_lookup_failed: " + lookupError);
              }
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
