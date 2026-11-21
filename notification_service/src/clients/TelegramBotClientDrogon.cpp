#include "clients/TelegramBotClient.h"

#include "common/http/DrogonHttpTransport.h"

#include <utility>

TelegramBotClient::TelegramBotClient(std::string botToken)
    : TelegramBotClient(
          std::move(botToken),
          std::make_shared<iot::http::DrogonHttpTransport>(
              "https://api.telegram.org")) {}
