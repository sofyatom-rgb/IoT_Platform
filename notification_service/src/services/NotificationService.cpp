#include "services/NotificationService.h"

#include <drogon/drogon.h>

#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

namespace {
std::string escapeHtml(const std::string &value) {
  return EmailTemplate::escapeHtml(value);
}

std::string formatTemperature(double value) {
  std::ostringstream stream;
  stream << std::fixed << std::setprecision(2) << value;
  std::string formatted = stream.str();
  const auto dot = formatted.find('.');
  if (dot != std::string::npos) {
    const auto last = formatted.find_last_not_of('0');
    if (last == dot) {
      formatted.erase(dot);
    } else {
      formatted.erase(last + 1);
    }
  }
  return formatted;
}

}  // namespace

NotificationService::NotificationService(
    TelegramBotClient telegramBotClient,
    std::optional<EmailService> emailService,
    std::shared_ptr<WorkerThreadPool> threadPool)
    : telegramBotClient_(std::move(telegramBotClient)),
      emailService_(std::move(emailService)),
      threadPool_(std::move(threadPool)) {}

bool NotificationService::reserveTemperatureAlert(int deviceId) const {
  if (deviceId <= 0) {
    return true;
  }
  std::lock_guard lock(alertState_->mutex);
  return alertState_->activeDevices.insert(deviceId).second;
}

void NotificationService::releaseTemperatureAlert(int deviceId) const {
  if (deviceId <= 0) {
    return;
  }
  std::lock_guard lock(alertState_->mutex);
  alertState_->activeDevices.erase(deviceId);
}

void NotificationService::clearTemperatureAlert(int deviceId) const {
  releaseTemperatureAlert(deviceId);
}

void NotificationService::sendEmailAlert(
    const std::string &email, const EmailAlertData &alert,
    std::function<void()> success,
    std::function<void(const std::string &)> error) const {
  if (email.empty()) {
    error("Recipient email is required");
    return;
  }
  if (!emailService_.has_value() || !emailService_->isEnabled()) {
    error("SMTP email notifications are disabled");
    return;
  }
  if (!threadPool_) {
    error("Email worker pool is not available");
    return;
  }

  const EmailService service = *emailService_;
  trantor::EventLoop *loop = trantor::EventLoop::getEventLoopOfCurrentThread();
  if (loop == nullptr) {
    loop = drogon::app().getLoop();
  }
  threadPool_->post([service, email, alert, loop,
                     success = std::move(success),
                     error = std::move(error)]() mutable {
    const EmailSendResult result = service.send(email, alert);
    if (!result.sent) {
      const std::string message = result.error;
      loop->queueInLoop(
          [error = std::move(error), message]() { error(message); });
      return;
    }
    loop->queueInLoop([success = std::move(success)]() { success(); });
  });
}

void NotificationService::sendTestEmail(
    const std::string &email, std::function<void()> success,
    std::function<void(const std::string &)> error) const {
  if (email.empty()) {
    error("Recipient email is required");
    return;
  }
  if (!emailService_.has_value() || !emailService_->isEnabled()) {
    error("SMTP email notifications are disabled");
    return;
  }
  if (!threadPool_) {
    error("Email worker pool is not available");
    return;
  }

  const EmailService service = *emailService_;
  trantor::EventLoop *loop = trantor::EventLoop::getEventLoopOfCurrentThread();
  if (loop == nullptr) {
    loop = drogon::app().getLoop();
  }
  threadPool_->post([service, email, loop,
                     success = std::move(success),
                     error = std::move(error)]() mutable {
    const EmailSendResult result = service.sendTestEmail(email);
    if (!result.sent) {
      const std::string message = result.error;
      loop->queueInLoop(
          [error = std::move(error), message]() { error(message); });
      return;
    }
    loop->queueInLoop([success = std::move(success)]() { success(); });
  });
}

void NotificationService::sendTemperatureAlert(
    int deviceId, const std::string &chatId, const std::string &email,
    const std::string &deviceName, double temperature, double threshold,
    std::function<void(const ChannelStatuses &)> success,
    std::function<void(const std::string &)> error) const {
  ChannelStatuses statuses;
  const bool wantTelegram = !chatId.empty();
  const bool wantEmail = !email.empty();
  const bool telegramEnabled = telegramBotClient_.isEnabled();
  const bool emailEnabled =
      emailService_.has_value() && emailService_->isEnabled();
  const bool tryTelegram = wantTelegram && telegramEnabled;
  const bool tryEmail = wantEmail && emailEnabled;

  if (!wantTelegram || !telegramEnabled) {
    statuses.telegram = "skipped";
  }
  if (!wantEmail || !emailEnabled) {
    statuses.email = "skipped";
  }

  const int totalToTry = (tryTelegram ? 1 : 0) + (tryEmail ? 1 : 0);
  if (totalToTry == 0) {
    if (!wantTelegram && !wantEmail) {
      error("No notification channel provided");
    } else {
      error("No notification channel is configured");
    }
    return;
  }

  if (!reserveTemperatureAlert(deviceId)) {
    statuses.telegram = tryTelegram ? "skipped_duplicate" : statuses.telegram;
    statuses.email = tryEmail ? "skipped_duplicate" : statuses.email;
    success(statuses);
    return;
  }

  auto done = std::make_shared<int>(0);
  auto state = std::make_shared<ChannelStatuses>(statuses);
  auto finishOne = [this, deviceId, done, state, totalToTry, tryTelegram,
                    tryEmail, success = std::move(success),
                    error = std::move(error)]() {
    ++(*done);
    if (*done < totalToTry) {
      return;
    }
    const bool telegramSent = state->telegram == "sent";
    const bool emailSent = state->email == "sent";
    if (telegramSent || emailSent) {
      success(*state);
      return;
    }

    releaseTemperatureAlert(deviceId);
    std::string parts;
    if (tryTelegram) {
      parts += "telegram: " + state->telegram;
    }
    if (tryEmail) {
      if (!parts.empty()) {
        parts += "; ";
      }
      parts += "email: " + state->email;
    }
    error(parts);
  };

  if (tryTelegram) {
    const std::string message =
        "⚠️ <b>Превышен порог температуры</b>\n\n"
        "Устройство: <b>" +
        escapeHtml(deviceName) + "</b>\n" +
        "Температура: <b>" + formatTemperature(temperature) + " °C</b>\n" +
        "Порог: " + formatTemperature(threshold) + " °C";
    telegramBotClient_.sendMessage(
        chatId, message,
        [state, finishOne]() {
          state->telegram = "sent";
          finishOne();
        },
        [state, finishOne](const std::string &messageError) {
          state->telegram = "failed: " + messageError;
          finishOne();
        });
  }

  if (tryEmail) {
    EmailAlertData alert;
    alert.eventType = "Превышен порог температуры";
    alert.sensorName = deviceName;
    alert.sensorId = deviceId > 0 ? std::to_string(deviceId) : "";
    alert.readingValue = formatTemperature(temperature);
    alert.unit = "°C";
    alert.threshold = formatTemperature(threshold);
    alert.severity = "WARNING";
    alert.description =
        "Показание датчика превысило установленный температурный порог.";
    sendEmailAlert(
        email, alert,
        [state, finishOne]() {
          state->email = "sent";
          finishOne();
        },
        [state, finishOne](const std::string &messageError) {
          state->email = "failed: " + messageError;
          finishOne();
        });
  }
}
