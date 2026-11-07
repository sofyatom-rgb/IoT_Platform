#include <drogon/drogon.h>

#include <cstdlib>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "clients/TelegramBotClient.h"
#include "clients/WorkerThreadPool.h"
#include "controllers/NotificationController.h"
#include "notifications/EmailConfig.h"
#include "notifications/EmailService.h"

int main(int, char *argv[]) {
  const auto configPath =
      std::filesystem::absolute(argv[0]).parent_path().parent_path() /
      "config" / "NotificationHTTPConfig.json";
  drogon::app().loadConfigFile(configPath.string());

  // Токен необязателен: Telegram-канал отключается, остаётся email.
  // Чтобы сервис работал только на email, TELEGRAM_BOT_TOKEN можно не задавать.
  const char *telegramBotToken = std::getenv("TELEGRAM_BOT_TOKEN");
  const std::string token =
      telegramBotToken ? std::string(telegramBotToken) : std::string();
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
  TelegramBotClient telegramClient{token, threadPool};
  NotificationService notificationService{
      std::move(telegramClient), std::move(emailService), threadPool};
  NotificationController controller(std::move(notificationService));
  controller.registerRoutes();

  drogon::app().run();
  return 0;
}
