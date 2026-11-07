#include <drogon/drogon.h>

#include <cstdlib>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "clients/TelegramBotClient.h"
#include "clients/WorkerThreadPool.h"
#include "common/http/DrogonHttpTransport.h"
#include "controllers/NotificationController.h"
#include "notifications/EmailConfig.h"
#include "notifications/EmailService.h"

int main(int, char *argv[]) {
  const auto configPath =
      std::filesystem::absolute(argv[0]).parent_path().parent_path() /
      "config" / "NotificationHTTPConfig.json";
  drogon::app().loadConfigFile(configPath.string());

  const char *telegramBotToken = std::getenv("TELEGRAM_BOT_TOKEN");
  const std::string token =
      telegramBotToken == nullptr ? std::string{} : telegramBotToken;
  if (token.empty()) {
    LOG_WARN << "TELEGRAM_BOT_TOKEN is not set, Telegram channel is disabled";
  }

  EmailConfig emailConfig = EmailConfig::fromEnvironment();
  std::optional<EmailService> emailService;
  if (!emailConfig.configurationError.empty()) {
    LOG_ERROR << "SMTP configuration error: "
              << emailConfig.configurationError;
  }
  if (!emailConfig.enabled) {
    LOG_WARN << "SMTP email notifications are disabled";
  } else {
    const std::string configError = emailConfig.validationError();
    if (!configError.empty() && emailConfig.configurationError.empty()) {
      LOG_ERROR << "SMTP configuration error: " << configError;
    }
    emailService.emplace(std::move(emailConfig));
  }

  auto threadPool = std::make_shared<WorkerThreadPool>(4);
  auto telegramTransport =
      std::make_shared<iot::http::DrogonHttpTransport>(
          "https://api.telegram.org");
  auto telegramClient =
      std::make_shared<TelegramBotClient>(token, std::move(telegramTransport));
  NotificationService notificationService{
      std::move(telegramClient), std::move(emailService), threadPool};
  NotificationController controller(std::move(notificationService));
  controller.registerRoutes();

  drogon::app().run();
  return 0;
}
