#pragma once

#include <functional>
#include <memory>
#include <string>

#include "clients/ITelegramBotClient.h"
#include "common/http/HttpTransport.h"

/// HTTP-клиент Telegram Bot API.
class TelegramBotClient : public ITelegramBotClient {
 public:
  /// Создаёт клиент с токеном бота и стандартным Telegram URL.
  explicit TelegramBotClient(std::string botToken);

  /// Создаёт клиент с токеном и внедрённым HTTP-транспортом.
  TelegramBotClient(std::string botToken,
                    std::shared_ptr<iot::http::IHttpTransport> transport);

  /// Отправляет текстовое сообщение в Telegram.
  void sendMessage(const std::string &chatId, const std::string &text,
                   std::function<void()> success,
                   std::function<void(const std::string &)> error) const override;

 private:
  std::string botToken_;
  std::shared_ptr<iot::http::IHttpTransport> transport_;
};
