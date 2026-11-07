#include "clients/TelegramBotClient.h"

#include <stdexcept>
#include <utility>

TelegramBotClient::TelegramBotClient(
    std::shared_ptr<ITelegramApi> telegramApi,
    std::shared_ptr<WorkerThreadPool> threadPool)
    : telegramApi_(std::move(telegramApi)), threadPool_(std::move(threadPool)) {
  if (!telegramApi_) {
    throw std::invalid_argument("TelegramBotClient requires a Telegram API");
  }
  if (!threadPool_) {
    throw std::invalid_argument(
        "TelegramBotClient requires a worker thread pool");
  }
}

void TelegramBotClient::sendMessage(
    const std::string &chatId, const std::string &text,
    std::function<void()> success,
    std::function<void(const std::string &)> error) const {
  sendMessage(chatId, text, {}, std::move(success), std::move(error));
}

void TelegramBotClient::sendMessage(
    const std::string &chatId, const std::string &text,
    const std::string &parseMode, std::function<void()> success,
    std::function<void(const std::string &)> error) const {
  if (!telegramApi_->isEnabled()) {
    if (error)
      error("Telegram bot is not configured");
    return;
  }

  const auto telegramApi = telegramApi_;
  threadPool_->post([telegramApi, chatId, text, parseMode,
                     success = std::move(success),
                     error = std::move(error)]() mutable {
    try {
      telegramApi->sendMessage(chatId, text, parseMode);
      if (success)
        success();
    } catch (const std::exception &exception) {
      if (error) {
        error("Telegram API error: " + std::string(exception.what()));
      }
    }
  });
}
