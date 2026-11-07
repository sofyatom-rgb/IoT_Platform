#pragma once

#include <functional>
#include <string>

// Порт Telegram API. Реальная реализация содержит HTTP-зависимость,
// тестовый fake позволяет проверить NotificationService без сети.
class ITelegramBotClient {
public:
  virtual ~ITelegramBotClient() = default;
  virtual bool isEnabled() const { return true; }
  virtual void
  sendMessage(const std::string &chatId, const std::string &text,
              std::function<void()> success,
              std::function<void(const std::string &)> error) const = 0;
  virtual void
  sendMessage(const std::string &chatId, const std::string &text,
              const std::string &parseMode, std::function<void()> success,
              std::function<void(const std::string &)> error) const {
    (void)parseMode;
    sendMessage(chatId, text, std::move(success), std::move(error));
  }
};
