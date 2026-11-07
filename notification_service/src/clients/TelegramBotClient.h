#pragma once

#include <functional>
#include <memory>
#include <string>

#include "clients/ITelegramBotClient.h"
#include "clients/ITelegramApi.h"
#include "clients/WorkerThreadPool.h"

class TelegramBotClient : public ITelegramBotClient {
 public:
  TelegramBotClient(std::shared_ptr<ITelegramApi> telegramApi,
                    std::shared_ptr<WorkerThreadPool> threadPool);

  bool isEnabled() const override { return telegramApi_->isEnabled(); }
  void sendMessage(const std::string &chatId, const std::string &text,
                   std::function<void()> success,
                   std::function<void(const std::string &)> error) const override;

 private:
  std::shared_ptr<ITelegramApi> telegramApi_;
  std::shared_ptr<WorkerThreadPool> threadPool_;
};
