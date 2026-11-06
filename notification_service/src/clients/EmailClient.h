#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>

#include "clients/TelegramThreadPool.h"

struct SmtpConfig {
  bool enabled = false;
  std::string host;
  int port = 587;
  std::string security = "starttls";  // none | starttls | tls
  std::string username;
  std::string password;
  std::string from;  // "Name <email@domain>" or just "email@domain"
  long timeoutSeconds = 20;
};

// SMTP-клиент на libcurl: блокирующий вызов в пуле потоков,
// результат возвращается в event loop Drogon через queueInLoop.
class EmailClient {
 public:
  EmailClient(SmtpConfig config,
              std::shared_ptr<TelegramThreadPool> threadPool);

  void sendMessage(const std::string &to, const std::string &subject,
                   const std::string &plainText, const std::string &html,
                   std::function<void()> success,
                   std::function<void(const std::string &)> error) const;

  bool isEnabled() const { return config_.enabled && !config_.host.empty(); }

  static std::string buildMime(const SmtpConfig &config,
                               const std::string &to,
                               const std::string &subject,
                               const std::string &plainText,
                               const std::string &html);

 private:
  SmtpConfig config_;
  std::shared_ptr<TelegramThreadPool> threadPool_;
};

// Вызывается один раз при старте (curl_global_init).
void initCurlGlobal();
