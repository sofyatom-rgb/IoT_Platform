#include <drogon/drogon.h>

#include "common/logging/DrogonLogging.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "clients/TelegramBotClient.h"
#include "controllers/NotificationController.h"

/// Запускает сервис Telegram-уведомлений.
int main(int, char *argv[]) {
  const auto configPath = std::filesystem::absolute(argv[0]).parent_path() /
                          "NotificationHTTPConfig.json";
  drogon::app().loadConfigFile(configPath.string());
  iot::logging::configureDrogonLogging(configPath.parent_path() / "logs",
                                       "notification_service");
  LOG_INFO << "[notification_service] Starting notification service";

  const char *telegramBotToken = std::getenv("TELEGRAM_BOT_TOKEN");
  if (!telegramBotToken || std::string(telegramBotToken).empty()) {
    LOG_FATAL << "[notification_service] TELEGRAM_BOT_TOKEN is required";
    return EXIT_FAILURE;
  }

  NotificationService notificationService{
      std::make_shared<TelegramBotClient>(telegramBotToken)};
  NotificationController controller(std::move(notificationService));
  controller.registerRoutes();

  LOG_INFO << "[notification_service] HTTP server is ready";
  drogon::app().run();
  return 0;
}
