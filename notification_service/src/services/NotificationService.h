#pragma once

#include <functional>
#include <optional>
#include <string>

#include "clients/EmailClient.h"
#include "clients/TelegramBotClient.h"
#include "clients/TelegramThreadPool.h"

// Статусы каналов: "sent" — доставлен, "skipped" — не настроен/не нужен,
// "failed: <подробности>" — попытка была, но сервер вернул ошибку.
struct ChannelStatuses {
  std::string telegram = "skipped";
  std::string email = "skipped";
};

class NotificationService {
 public:
  NotificationService(TelegramBotClient telegramBotClient,
                      std::optional<EmailClient> emailClient,
                      std::shared_ptr<TelegramThreadPool> threadPool);

  // success вызывается, когда хотя бы один отправляемый канал доставил.
  // error вызывается, когда все отправляемые каналы упали или ни один
  // канал не настроен.
  void sendTemperatureAlert(
      const std::string &chatId, const std::string &email,
      const std::string &deviceName, double temperature, double threshold,
      std::function<void(const ChannelStatuses &)> success,
      std::function<void(const std::string &)> error) const;

 private:
  TelegramBotClient telegramBotClient_;
  std::optional<EmailClient> emailClient_;
  std::shared_ptr<TelegramThreadPool> threadPool_;
};
