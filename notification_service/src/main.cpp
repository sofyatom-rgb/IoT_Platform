#include <drogon/drogon.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "clients/TelegramBotClient.h"
#include "clients/TelegramThreadPool.h"
#include "controllers/NotificationController.h"

int main(int, char *argv[]) {
  const char *telegramBotToken = std::getenv("TELEGRAM_BOT_TOKEN");
  if (!telegramBotToken || std::string(telegramBotToken).empty()) {
    std::cerr << "TELEGRAM_BOT_TOKEN is required\n";
    return EXIT_FAILURE;
  }

  const auto configPath = std::filesystem::absolute(argv[0]).parent_path() /
                          "NotificationHTTPConfig.json";
  drogon::app().loadConfigFile(configPath.string());

  auto threadPool = std::make_shared<TelegramThreadPool>(4);
  NotificationService notificationService{
      TelegramBotClient{telegramBotToken, threadPool}};
  NotificationController controller(std::move(notificationService));
  controller.registerRoutes();

  drogon::app().run();
  return 0;
}
