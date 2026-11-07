#pragma once

#include <string>

class ITelegramApi {
public:
  virtual ~ITelegramApi() = default;
  virtual bool isEnabled() const = 0;
  virtual void sendMessage(const std::string &chatId, const std::string &text,
                           const std::string &parseMode) const = 0;
};
