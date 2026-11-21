#pragma once

#include <functional>
#include <string>

/// Контракт отправки сообщений через Telegram Bot API.
class ITelegramBotClient {
 public:
  /// Позволяет удалять реализацию через указатель на интерфейс.
  virtual ~ITelegramBotClient() = default;

  /// Отправляет текстовое сообщение в указанный чат.
  virtual void sendMessage(const std::string &chatId, const std::string &text,
                           std::function<void()> success,
                           std::function<void(const std::string &)> error) const = 0;
};
