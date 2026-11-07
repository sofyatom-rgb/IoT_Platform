#include "clients/TgBotApi.h"

#include <stdexcept>
#include <utility>

TgBotApi::TgBotApi(std::string botToken) {
  if (!botToken.empty()) {
    bot_ = std::make_unique<TgBot::Bot>(std::move(botToken));
  }
}

void TgBotApi::sendMessage(const std::string &chatId,
                           const std::string &text) const {
  if (!bot_) {
    throw std::runtime_error("Telegram bot is not configured");
  }

  std::lock_guard lock(mutex_);
  bot_->getApi().sendMessage(chatId, text);
}
