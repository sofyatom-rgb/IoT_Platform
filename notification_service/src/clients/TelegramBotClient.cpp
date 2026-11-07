#include "clients/TelegramBotClient.h"

#include <stdexcept>
#include <utility>

TelegramBotClient::TelegramBotClient(
    std::string botToken,
    std::shared_ptr<iot::http::IHttpTransport> transport)
    : botToken_(std::move(botToken)), transport_(std::move(transport)) {
  if (!transport_) {
    throw std::invalid_argument("TelegramBotClient requires an HTTP transport");
  }
}

void TelegramBotClient::sendMessage(
    const std::string &chatId, const std::string &text,
    std::function<void()> success,
    std::function<void(const std::string &)> error) const {
  if (botToken_.empty()) {
    if (error) error("Telegram bot is not configured");
    return;
  }

  iot::http::Request request;
  request.method = iot::http::Method::Post;
  request.path = "/bot" + botToken_ + "/sendMessage";
  request.jsonBody = {{"chat_id", chatId}, {"text", text}};

  transport_->send(
      request,
      [success = std::move(success), error = std::move(error)](
          iot::http::Result result,
          std::optional<iot::http::Response> response) {
        if (result != iot::http::Result::Ok || !response) {
          const int status = response ? response->statusCode : 0;
          if (error) {
            error("Telegram API request failed (HTTP " +
                  std::to_string(status) + ")");
          }
          return;
        }

        if (!response->jsonParsed) {
          if (response->statusCode != 200) {
            if (error) {
              error("Telegram API request failed (HTTP " +
                    std::to_string(response->statusCode) + ")");
            }
            return;
          }
          const std::string suffix = response->jsonParseError.empty()
                                         ? std::string{}
                                         : ": " + response->jsonParseError;
          if (error) error("Could not parse Telegram API response" + suffix);
          return;
        }

        const bool *ok = iot::http::boolField(response->jsonBody, "ok");
        if (response->statusCode != 200 || ok == nullptr || !*ok) {
          const auto *description =
              iot::http::stringField(response->jsonBody, "description");
          if (description != nullptr && !description->empty()) {
            if (error) error("Telegram API error: " + *description);
          } else if (error) {
            error("Telegram API request failed (HTTP " +
                  std::to_string(response->statusCode) + ")");
          }
          return;
        }

        if (success) success();
      });
}
