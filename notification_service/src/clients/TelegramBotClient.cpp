#include "clients/TelegramBotClient.h"

#include <drogon/drogon.h>
#include <tgbot/TgException.h>

#include <cctype>
#include <cstdint>
#include <exception>
#include <string>
#include <utility>
#include <variant>

namespace {
std::variant<std::int64_t, std::string> parseChatId(
    const std::string &chatId) {
  const std::size_t start = (!chatId.empty() && chatId.front() == '-') ? 1 : 0;
  if (start == chatId.size()) {
    return chatId;
  }
  for (std::size_t i = start; i < chatId.size(); ++i) {
    if (std::isdigit(static_cast<unsigned char>(chatId[i])) == 0) {
      return chatId;
    }
  }
  try {
    return static_cast<std::int64_t>(std::stoll(chatId));
  } catch (const std::exception &) {
    return chatId;
  }
}
}  // namespace

TelegramBotClient::TelegramBotClient(
    std::string botToken, std::shared_ptr<TelegramThreadPool> threadPool)
    : bot_(botToken.empty() ? nullptr
                            : std::make_shared<TgBot::Bot>(std::move(botToken))),
      threadPool_(std::move(threadPool)) {}

void TelegramBotClient::sendMessage(
    const std::string &chatId, const std::string &text,
    std::function<void()> success,
    std::function<void(const std::string &)> error) const {
  const auto bot = bot_;
  if (!bot || !threadPool_) {
    error("Telegram bot is not configured");
    return;
  }

  trantor::EventLoop *loop = trantor::EventLoop::getEventLoopOfCurrentThread();
  if (loop == nullptr) {
    loop = drogon::app().getLoop();
  }

  threadPool_->post([bot, loop, chatId, text,
                     success = std::move(success),
                     error = std::move(error)]() mutable {
    try {
      TgBot::SendMessageArgs args;
      args.chatId = parseChatId(chatId);
      args.text = text;
      args.parseMode = "HTML";
      bot->getApi().sendMessage(args);
      loop->queueInLoop([success = std::move(success)]() { success(); });
    } catch (const TgBot::TgException &exception) {
      const std::string message =
          "Telegram API error: " + std::string(exception.what());
      loop->queueInLoop(
          [error = std::move(error), message]() { error(message); });
    } catch (const std::exception &exception) {
      const std::string message =
          "Telegram API request failed: " + std::string(exception.what());
      loop->queueInLoop(
          [error = std::move(error), message]() { error(message); });
    }
  });
}
