#include "clients/TelegramBotClient.h"

#include "common/http/DrogonHttpTransport.h"

#include <utility>

TelegramBotClient::TelegramBotClient(std::string botToken)
    : TelegramBotClient(
          createHttpApi(std::move(botToken),
                        std::make_shared<iot::http::DrogonHttpTransport>(
                            "https://api.telegram.org")),
          // HTTP-транспорт асинхронный, поэтому ждём ответ в рабочем пуле.
          std::make_shared<WorkerThreadPool>(2)) {}
