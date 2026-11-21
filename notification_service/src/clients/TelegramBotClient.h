#pragma once

#include <functional>
#include <memory>
#include <string>

#include "clients/ITelegramApi.h"
#include "clients/ITelegramBotClient.h"
#include "clients/WorkerThreadPool.h"
#include "common/http/HttpTransport.h"

class TelegramBotClient : public ITelegramBotClient {
public:
  TelegramBotClient(std::shared_ptr<ITelegramApi> telegramApi,
                    std::shared_ptr<WorkerThreadPool> threadPool);

  // Совместимость со старыми вызывающими: токен и HTTP-транспорт.
  explicit TelegramBotClient(std::string botToken);

  TelegramBotClient(std::string botToken,
                    std::shared_ptr<iot::http::IHttpTransport> transport);

  /// Создаёт реализацию Telegram API поверх внутреннего HTTP-транспорта.
  static std::shared_ptr<ITelegramApi>
  createHttpApi(std::string botToken,
                std::shared_ptr<iot::http::IHttpTransport> transport);

  bool isEnabled() const override { return telegramApi_->isEnabled(); }
  void
  sendMessage(const std::string &chatId, const std::string &text,
              std::function<void()> success,
              std::function<void(const std::string &)> error) const override;
  void
  sendMessage(const std::string &chatId, const std::string &text,
              const std::string &parseMode, std::function<void()> success,
              std::function<void(const std::string &)> error) const override;

private:
  std::shared_ptr<ITelegramApi> telegramApi_;
  std::shared_ptr<WorkerThreadPool> threadPool_;
  bool synchronous_ = false;
};
