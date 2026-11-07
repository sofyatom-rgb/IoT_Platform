#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <mutex>
#include <unordered_set>

#include "clients/TelegramBotClient.h"
#include "clients/WorkerThreadPool.h"
#include "notifications/EmailService.h"

// Статусы каналов: "sent" — доставлен, "skipped" — не настроен/не нужен,
// "failed: <подробности>" — попытка была, но сервер вернул ошибку.
struct ChannelStatuses {
  std::string telegram = "skipped";
  std::string email = "skipped";
};

class NotificationService {
 public:
  NotificationService(TelegramBotClient telegramBotClient,
                      std::optional<EmailService> emailService,
                      std::shared_ptr<WorkerThreadPool> threadPool);

  // success вызывается, когда хотя бы один отправляемый канал доставил.
  // error вызывается, когда все отправляемые каналы упали или ни один
  // канал не настроен.
  void sendTemperatureAlert(
      int deviceId, const std::string &chatId, const std::string &email,
      const std::string &deviceName, double temperature, double threshold,
      std::function<void(const ChannelStatuses &)> success,
      std::function<void(const std::string &)> error) const;

  void clearTemperatureAlert(int deviceId) const;
  void sendEmailAlert(
      const std::string &email, const EmailAlertData &alert,
      std::function<void()> success,
      std::function<void(const std::string &)> error) const;
  void sendTestEmail(
      const std::string &email, std::function<void()> success,
      std::function<void(const std::string &)> error) const;

 private:
  struct AlertState {
    std::mutex mutex;
    std::unordered_set<int> activeDevices;
  };

  bool reserveTemperatureAlert(int deviceId) const;
  void releaseTemperatureAlert(int deviceId) const;

  TelegramBotClient telegramBotClient_;
  std::optional<EmailService> emailService_;
  std::shared_ptr<WorkerThreadPool> threadPool_;
  std::shared_ptr<AlertState> alertState_ = std::make_shared<AlertState>();
};
