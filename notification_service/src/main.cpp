#include <drogon/drogon.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "clients/TelegramBotClient.h"
#include "common/http/DrogonHttpTransport.h"
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
  NotificationService notificationService{
      std::make_shared<TelegramBotClient>(
          telegramBotToken,
          std::make_shared<iot::http::DrogonHttpTransport>(
              "https://api.telegram.org"))};
  NotificationController controller(std::move(notificationService));
  controller.registerRoutes();

  drogon::app().run();
  return 0;
}
