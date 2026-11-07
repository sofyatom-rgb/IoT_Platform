#include "services/NotificationService.h"

#include <drogon/drogon.h>

#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>
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

trantor::EventLoop *currentEventLoop() {
  auto *loop = trantor::EventLoop::getEventLoopOfCurrentThread();
  return loop != nullptr ? loop : drogon::app().getLoop();
}
}  // namespace

NotificationService::NotificationService(
    std::shared_ptr<ITelegramBotClient> telegramBotClient)
    : NotificationService(std::move(telegramBotClient), std::nullopt, {}) {}

NotificationService::NotificationService(
    std::shared_ptr<ITelegramBotClient> telegramBotClient,
    std::optional<EmailService> emailService,
    std::shared_ptr<WorkerThreadPool> threadPool)
    : telegramBotClient_(std::move(telegramBotClient)),
      emailService_(std::move(emailService)),
      threadPool_(std::move(threadPool)) {
  if (!telegramBotClient_) {
    throw std::invalid_argument("NotificationService requires a Telegram client");
  }
}

bool NotificationService::reserveTemperatureAlert(int deviceId) const {
  if (deviceId <= 0) return true;
  std::lock_guard lock(alertState_->mutex);
  return alertState_->activeDevices.insert(deviceId).second;
}

void NotificationService::releaseTemperatureAlert(int deviceId) const {
  if (deviceId <= 0) return;
  std::lock_guard lock(alertState_->mutex);
  alertState_->activeDevices.erase(deviceId);
}

void NotificationService::clearTemperatureAlert(int deviceId) const {
  releaseTemperatureAlert(deviceId);
}

void NotificationService::sendTemperatureAlert(
    const std::string &chatId, const std::string &deviceName,
    double temperature, double threshold, std::function<void()> success,
    std::function<void(const std::string &)> error) const {
  if (chatId.empty()) {
    if (error) error("Telegram chat ID is required");
    return;
  }
  if (!telegramBotClient_->isEnabled()) {
    if (error) error("Telegram bot is not configured");
    return;
  }
  const std::string message =
      "Внимание! На устройстве \"" + deviceName + "\" температура " +
      std::to_string(temperature) + " °C превысила порог " +
      std::to_string(threshold) + " °C.";
  telegramBotClient_->sendMessage(chatId, message, std::move(success),
                                  std::move(error));
}

void NotificationService::sendTemperatureAlert(
    int deviceId, const std::string &chatId, const std::string &email,
    const std::string &deviceName, double temperature, double threshold,
    std::function<void(const ChannelStatuses &)> success,
    std::function<void(const std::string &)> error) const {
  ChannelStatuses initial;
  const bool tryTelegram = !chatId.empty() && telegramBotClient_->isEnabled();
  const bool tryEmail = !email.empty() && emailService_.has_value() &&
                        emailService_->isEnabled();
  if (!tryTelegram && !tryEmail) {
    if (chatId.empty() && email.empty()) {
      if (error) error("No notification channel provided");
    } else if (chatId.empty() || !telegramBotClient_->isEnabled()) {
      if (!tryEmail) {
        if (error) error("No notification channel is configured");
      }
    } else if (!tryTelegram && !tryEmail && error) {
      error("No notification channel is configured");
    }
    return;
  }

  if (!reserveTemperatureAlert(deviceId)) {
    initial.telegram = tryTelegram ? "skipped_duplicate" : "skipped";
    initial.email = tryEmail ? "skipped_duplicate" : "skipped";
    if (success) success(initial);
    return;
  }

  const int total = static_cast<int>(tryTelegram) + static_cast<int>(tryEmail);
  auto completed = std::make_shared<int>(0);
  auto statuses = std::make_shared<ChannelStatuses>(initial);
  auto finish = [this, deviceId, total, completed, statuses, tryTelegram,
                 tryEmail, success = std::move(success),
                 error = std::move(error)]() mutable {
    ++*completed;
    if (*completed != total) return;
    if (statuses->telegram == "sent" || statuses->email == "sent") {
      if (success) success(*statuses);
      return;
    }

    releaseTemperatureAlert(deviceId);
    std::string details;
    if (tryTelegram) details = "telegram: " + statuses->telegram;
    if (tryEmail) {
      if (!details.empty()) details += "; ";
      details += "email: " + statuses->email;
    }
    if (error) error(details);
  };

  if (tryTelegram) {
    const std::string message =
        "⚠️ <b>Превышен порог температуры</b>\n\n"
        "Устройство: <b>" +
        escapeHtml(deviceName) + "</b>\n" +
        "Температура: <b>" + formatTemperature(temperature) + " °C</b>\n" +
        "Порог: " + formatTemperature(threshold) + " °C";
    telegramBotClient_->sendMessage(
        chatId, message,
        [statuses, finish]() mutable {
          statuses->telegram = "sent";
          finish();
        },
        [statuses, finish](const std::string &messageError) mutable {
          statuses->telegram = "failed: " + messageError;
          finish();
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
        [statuses, finish]() mutable {
          statuses->email = "sent";
          finish();
        },
        [statuses, finish](const std::string &messageError) mutable {
          statuses->email = "failed: " + messageError;
          finish();
        });
  }
}

void NotificationService::sendEmailAlert(
    const std::string &email, const EmailAlertData &alert,
    std::function<void()> success,
    std::function<void(const std::string &)> error) const {
  if (email.empty()) {
    if (error) error("Recipient email is required");
    return;
  }
  if (!emailService_.has_value() || !emailService_->isEnabled()) {
    if (error) error("SMTP email notifications are disabled");
    return;
  }
  if (!threadPool_) {
    if (error) error("Email worker pool is not available");
    return;
  }
  auto *loop = currentEventLoop();
  if (loop == nullptr) {
    if (error) error("Drogon event loop is not available");
    return;
  }

  const EmailService service = *emailService_;
  threadPool_->post([service, email, alert, loop,
                     success = std::move(success),
                     error = std::move(error)]() mutable {
    const EmailSendResult result = service.send(email, alert);
    if (!result.sent) {
      const std::string message = result.error;
      loop->queueInLoop(
          [error = std::move(error), message]() mutable {
            if (error) error(message);
          });
      return;
    }
    loop->queueInLoop([success = std::move(success)]() mutable {
      if (success) success();
    });
  });
}

void NotificationService::sendTestEmail(
    const std::string &email, std::function<void()> success,
    std::function<void(const std::string &)> error) const {
  if (email.empty()) {
    if (error) error("Recipient email is required");
    return;
  }
  if (!emailService_.has_value() || !emailService_->isEnabled()) {
    if (error) error("SMTP email notifications are disabled");
    return;
  }
  if (!threadPool_) {
    if (error) error("Email worker pool is not available");
    return;
  }
  auto *loop = currentEventLoop();
  if (loop == nullptr) {
    if (error) error("Drogon event loop is not available");
    return;
  }

  const EmailService service = *emailService_;
  threadPool_->post([service, email, loop,
                     success = std::move(success),
                     error = std::move(error)]() mutable {
    const EmailSendResult result = service.sendTestEmail(email);
    if (!result.sent) {
      const std::string message = result.error;
      loop->queueInLoop(
          [error = std::move(error), message]() mutable {
            if (error) error(message);
          });
      return;
    }
    loop->queueInLoop([success = std::move(success)]() mutable {
      if (success) success();
    });
  });
}
