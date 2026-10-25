#include "clients/TelegramBotClient.h"

#include <sstream>
#include <utility>

TelegramBotClient::TelegramBotClient(std::string botToken)
    : botToken_(std::move(botToken)) {}

void TelegramBotClient::sendMessage(
    const std::string &chatId,
    const std::string &text,
    std::function<void()> success,
    std::function<void(const std::string &)> error) const {
  auto client = drogon::HttpClient::newHttpClient("https://api.telegram.org");
  auto request = drogon::HttpRequest::newHttpRequest();
  request->setMethod(drogon::Post);
  request->setPath("/bot" + botToken_ + "/sendMessage");
  request->setContentTypeCode(drogon::CT_APPLICATION_JSON);

  Json::Value body;
  body["chat_id"] = chatId;
  body["text"] = text;
  request->setBody(body.toStyledString());

  client->sendRequest(
      request,
      [success = std::move(success), error = std::move(error)](
          drogon::ReqResult result,
          const drogon::HttpResponsePtr &response) {
        if (result != drogon::ReqResult::Ok || !response) {
          const auto status =
              response ? static_cast<int>(response->getStatusCode()) : 0;
          error("Telegram API request failed (HTTP " +
                std::to_string(status) + ")");
          return;
        }

        Json::CharReaderBuilder builder;
        Json::Value payload;
        std::string parseErrors;
        const std::string responseBody(response->getBody());
        std::istringstream input(responseBody);
        if (!Json::parseFromStream(builder, input, &payload, &parseErrors)) {
          if (response->getStatusCode() != drogon::k200OK) {
            error("Telegram API request failed (HTTP " +
                  std::to_string(static_cast<int>(response->getStatusCode())) +
                  ")");
            return;
          }
          error("Could not parse Telegram API response: " + parseErrors);
          return;
        }
        if (!payload["ok"].isBool() || !payload["ok"].asBool()) {
          const auto description = payload["description"].asString();
          error(description.empty() ? "Telegram API rejected the message"
                                    : "Telegram API error: " + description);
          return;
        }

        success();
      });
}
