#pragma once

#include <functional>
#include <memory>
#include <string>

#include "clients/ITelegramBotClient.h"
#include "common/http/HttpTransport.h"

class TelegramBotClient : public ITelegramBotClient {
 public:
  TelegramBotClient(
      std::string botToken,
      std::shared_ptr<iot::http::IHttpTransport> transport);

  bool isEnabled() const override { return !botToken_.empty(); }
  void sendMessage(const std::string &chatId, const std::string &text,
                   std::function<void()> success,
                   std::function<void(const std::string &)> error) const override;

 private:
  std::string botToken_;
  std::shared_ptr<iot::http::IHttpTransport> transport_;
};
