#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_set>

#include "clients/ITelegramBotClient.h"
#include "clients/WorkerThreadPool.h"
#include "notifications/EmailService.h"
#include "notifications/TelegramMessageTemplates.h"

struct ChannelStatuses {
  std::string telegram = "skipped";
  std::string email = "skipped";
};

class NotificationService {
public:
  explicit NotificationService(
      std::shared_ptr<ITelegramBotClient> telegramBotClient);
  NotificationService(std::shared_ptr<ITelegramBotClient> telegramBotClient,
                      std::optional<EmailService> emailService,
                      std::shared_ptr<WorkerThreadPool> threadPool);
  NotificationService(std::shared_ptr<ITelegramBotClient> telegramBotClient,
                      std::optional<EmailService> emailService,
                      std::shared_ptr<WorkerThreadPool> threadPool,
                      TelegramMessageTemplates telegramTemplates);

  void
  sendTemperatureAlert(const std::string &chatId, const std::string &deviceName,
                       double temperature, double threshold,
                       std::function<void()> success,
                       std::function<void(const std::string &)> error) const;
  void
  sendTemperatureAlert(int deviceId, const std::string &chatId,
                       const std::string &email, const std::string &deviceName,
                       double temperature, double threshold,
                       std::function<void(const ChannelStatuses &)> success,
                       std::function<void(const std::string &)> error) const;
  void clearTemperatureAlert(int deviceId) const;
  void sendEmailAlert(const std::string &email, const EmailAlertData &alert,
                      std::function<void()> success,
                      std::function<void(const std::string &)> error) const;
  void sendTestEmail(const std::string &email, std::function<void()> success,
                     std::function<void(const std::string &)> error) const;

private:
  struct AlertState {
    std::mutex mutex;
    std::unordered_set<int> activeDevices;
  };

  bool reserveTemperatureAlert(int deviceId) const;
  void releaseTemperatureAlert(int deviceId) const;

  std::shared_ptr<ITelegramBotClient> telegramBotClient_;
  std::optional<EmailService> emailService_;
  std::shared_ptr<WorkerThreadPool> threadPool_;
  TelegramMessageTemplates telegramTemplates_;
  std::shared_ptr<AlertState> alertState_ = std::make_shared<AlertState>();
};
