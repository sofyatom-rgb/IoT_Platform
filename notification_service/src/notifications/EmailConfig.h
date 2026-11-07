#pragma once

#include <string>

struct EmailConfig {
  bool enabled = false;
  std::string host = "smtp.gmail.com";
  unsigned port = 587;
  std::string security = "starttls";
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
