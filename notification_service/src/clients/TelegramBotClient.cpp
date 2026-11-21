#include "clients/TelegramBotClient.h"

#include <stdexcept>
#include <utility>

namespace {
class LegacyTelegramApi : public ITelegramApi {
 public:
  LegacyTelegramApi(std::string token,
                    std::shared_ptr<iot::http::IHttpTransport> transport)
      : token_(std::move(token)), transport_(std::move(transport)) {
    if (!transport_) {
      throw std::invalid_argument("TelegramBotClient requires an HTTP transport");
    }
  }

  bool isEnabled() const override { return true; }

  void sendMessage(const std::string &chatId, const std::string &text,
                   const std::string &) const override {
    iot::http::Request request;
    request.method = iot::http::Method::Post;
    request.path = "/bot" + token_ + "/sendMessage";
    request.jsonBody = {{"chat_id", chatId}, {"text", text}};
    bool finished = false;
    std::string failure;
    transport_->send(
        request,
        [&](iot::http::Result result,
            std::optional<iot::http::Response> response) {
          finished = true;
          if (result != iot::http::Result::Ok || !response) {
            failure = "HTTP " +
                      std::to_string(response ? response->statusCode : 0);
            return;
          }
          if (!response->jsonParsed) {
            failure = response->statusCode == 200
                          ? "Could not parse Telegram API response: " +
                                response->jsonParseError
                          : "HTTP " + std::to_string(response->statusCode);
            return;
          }
          const bool *ok = iot::http::boolField(response->jsonBody, "ok");
          if (!ok || !*ok) {
            const auto *description =
                iot::http::stringField(response->jsonBody, "description");
            failure = description && !description->empty()
                          ? "Telegram API error: " + *description
                          : "Telegram API rejected the message";
            return;
          }
          if (response->statusCode != 200) {
            failure = "HTTP " + std::to_string(response->statusCode);
          }
        });
    if (!finished) {
      failure = "HTTP 0";
    }
    if (!failure.empty()) {
      throw std::runtime_error(failure);
    }
  }

 private:
  std::string token_;
  std::shared_ptr<iot::http::IHttpTransport> transport_;
};
}  // namespace

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

TelegramBotClient::TelegramBotClient(
    std::string botToken,
    std::shared_ptr<iot::http::IHttpTransport> transport)
    : telegramApi_(std::make_shared<LegacyTelegramApi>(std::move(botToken),
                                                       std::move(transport))),
      synchronous_(true) {}

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
  if (synchronous_) {
    try {
      telegramApi->sendMessage(chatId, text, parseMode);
      if (success) success();
    } catch (const std::exception &exception) {
      if (error) error("Telegram API error: " + std::string(exception.what()));
    }
    return;
  }

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
