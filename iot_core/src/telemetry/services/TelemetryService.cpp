#include "telemetry/services/TelemetryService.h"

#include <drogon/drogon.h>

#include <memory>
#include <string>
#include <utility>

#include "devices/services/DevicesService.h"
#include "users/services/UsersService.h"

namespace {
void logNotificationError(const std::string &error) {
  LOG_ERROR << "Could not send temperature notification: " << error;
}

std::string summarizeChannels(const ChannelStatuses &channels) {
  auto isFailed = [](const std::string &s) {
    return s.rfind("failed", 0) == 0;
  };
  const bool anySent =
      (channels.telegram == "sent" || channels.email == "sent");
  const bool anyFailed =
      isFailed(channels.telegram) || isFailed(channels.email);
  if (anySent && !anyFailed) {
    return "sent";
  }
  const bool duplicateOnly =
      (channels.telegram == "skipped_duplicate" ||
       channels.email == "skipped_duplicate") &&
      (channels.telegram.empty() || channels.telegram == "skipped" ||
       channels.telegram == "skipped_duplicate") &&
      (channels.email.empty() || channels.email == "skipped" ||
       channels.email == "skipped_duplicate");
  if (duplicateOnly) {
    return "duplicate_suppressed";
  }
  // notification_service возвращает 200 только при доставке хотя бы в один канал.
  std::string details;
  if (!channels.telegram.empty() && channels.telegram != "skipped") {
    details += "telegram=" + channels.telegram;
  }
  if (!channels.email.empty() && channels.email != "skipped") {
    if (!details.empty()) {
      details += ", ";
    }
    details += "email=" + channels.email;
  }
  return "sent_partial" + (details.empty() ? "" : ": " + details);
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
          notificationClient.clearTemperatureAlert(
              deviceId, [finishSuccess]() { (*finishSuccess)("not_required"); },
              [finishSuccess](const std::string &error) {
                LOG_WARN << "Could not clear temperature alert state: "
                         << error;
                (*finishSuccess)("not_required");
              });
          return;
        }

        DevicesService devices(db);
        devices.getNotificationInfo(
            deviceId, [db, deviceId, temperature, threshold,
                       notificationClient, finishSuccess](
                int ownerId, const std::string &deviceName) {
              LOG_INFO << "Temperature threshold exceeded for device; "
                          "resolved owner";
              UsersService users(db);
              users.getNotificationTargets(
                  ownerId, [deviceId, temperature, threshold,
                            notificationClient, deviceName, finishSuccess](
                      const NotificationTargets &targets) {
                    if (targets.chatId.empty() && targets.email.empty()) {
                      LOG_WARN << "Temperature threshold exceeded, but user "
                                  "has no Telegram chat ID or email";
                      (*finishSuccess)("no_contact_info");
                      return;
                    }

                    notificationClient.sendTemperatureAlert(
                        deviceId, targets.chatId, targets.email, deviceName,
                        temperature,
                        threshold,
                        [finishSuccess](const ChannelStatuses &channels) {
                          LOG_INFO << "Temperature notification accepted by "
                                      "notification service";
                          (*finishSuccess)(summarizeChannels(channels));
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
