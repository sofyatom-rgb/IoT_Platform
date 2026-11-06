#include <drogon/drogon.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "clients/EmailClient.h"
#include "clients/TelegramBotClient.h"
#include "clients/TelegramThreadPool.h"
#include "controllers/NotificationController.h"

namespace {
SmtpConfig readSmtpConfig() {
  SmtpConfig smtp;
  const auto config = drogon::app().getCustomConfig()["smtp"];
  if (!config.isObject()) {
    return smtp;
  }
  smtp.enabled = config["enabled"].asBool();
  smtp.host = config["host"].asString();
  if (config["port"].isInt()) {
    smtp.port = config["port"].asInt();
  } else if (config["port"].isNumeric()) {
    smtp.port = static_cast<int>(config["port"].asDouble());
  }
  if (config["security"].isString() && !config["security"].asString().empty()) {
    smtp.security = config["security"].asString();
  }
  smtp.username = config["username"].asString();
  // Секрет можно переопределить через env (как TELEGRAM_BOT_TOKEN).
  if (const char *envPassword = std::getenv("SMTP_PASSWORD")) {
    smtp.password = envPassword;
  } else {
    smtp.password = config["password"].asString();
  }
  smtp.from = config["from"].asString();
  if (config["timeout_seconds"].isNumeric()) {
    smtp.timeoutSeconds = config["timeout_seconds"].asInt64();
  }
  return smtp;
}
}  // namespace

int main(int, char *argv[]) {
  initCurlGlobal();

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

  const SmtpConfig smtp = readSmtpConfig();
  if (!smtp.enabled || smtp.host.empty()) {
    LOG_WARN << "SMTP is not configured, email channel is disabled";
  }

  auto threadPool = std::make_shared<TelegramThreadPool>(4);
  TelegramBotClient telegramClient{token, threadPool};
  std::optional<EmailClient> emailClient;
  if (smtp.enabled && !smtp.host.empty()) {
    emailClient.emplace(smtp, threadPool);
  }
  NotificationService notificationService{
      std::move(telegramClient), std::move(emailClient), threadPool};
  NotificationController controller(std::move(notificationService));
  controller.registerRoutes();

  drogon::app().run();
  return 0;
}
