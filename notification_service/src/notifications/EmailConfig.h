#pragma once

#include <string>

struct EmailConfig {
  bool enabled = false;
  // Yandex SMTP: implicit TLS is the provider's recommended setting.
  std::string host = "smtp.yandex.ru";
  unsigned port = 465;
  std::string security = "tls";
  std::string username;
  std::string password;
  std::string senderName = "IoT Monitor";
  std::string fromEmail;
  std::string platformUrl;
  long timeoutSeconds = 20;
  std::string configurationError;

  static EmailConfig fromEnvironment();
  std::string validationError() const;
};
