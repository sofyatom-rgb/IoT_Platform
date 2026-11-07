#include "clients/TgBotApi.h"

#include <stdexcept>
#include <utility>

TgBotApi::TgBotApi(std::string botToken) {
  if (!botToken.empty()) {
    bot_ = std::make_unique<TgBot::Bot>(std::move(botToken));
  }
}

void TgBotApi::sendMessage(const std::string &chatId, const std::string &text,
                           const std::string &parseMode) const {
  if (!bot_) {
    throw std::runtime_error("Telegram bot is not configured");
  }

  std::lock_guard lock(mutex_);
  TgBot::SendMessageArgs request;
  request.chatId = chatId;
  request.text = text;
  request.parseMode = parseMode;
  bot_->getApi().sendMessage(request);
}
