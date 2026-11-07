#pragma once

#include <memory>
#include <mutex>
#include <string>

#include <tgbot/Bot.h>

#include "clients/ITelegramApi.h"

class TgBotApi final : public ITelegramApi {
public:
  explicit TgBotApi(std::string botToken);

  bool isEnabled() const override { return static_cast<bool>(bot_); }
  void sendMessage(const std::string &chatId, const std::string &text,
                   const std::string &parseMode) const override;

private:
  std::unique_ptr<TgBot::Bot> bot_;
  mutable std::mutex mutex_;
};
