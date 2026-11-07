#include "notifications/EmailConfig.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <limits>
#include <string>

namespace {
std::string environmentValue(const char *name) {
  const char *value = std::getenv(name);
  return value == nullptr ? std::string{} : std::string(value);
}

bool parseBoolean(const std::string &value, bool &result) {
  std::string normalized = value;
  std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                 [](unsigned char ch) {
                   return static_cast<char>(std::tolower(ch));
                 });
  if (normalized == "true" || normalized == "1" || normalized == "yes" ||
      normalized == "on") {
    result = true;
    return true;
  }
  if (normalized == "false" || normalized == "0" || normalized == "no" ||
      normalized == "off") {
    result = false;
    return true;
  }
  return false;
}

bool parseUnsigned(const std::string &value, unsigned &result) {
  if (value.empty() ||
      !std::all_of(value.begin(), value.end(),
                   [](unsigned char ch) { return std::isdigit(ch) != 0; })) {
    return false;
  }
  try {
    const auto parsed = std::stoul(value);
    if (parsed > std::numeric_limits<unsigned>::max()) {
      return false;
    }
    result = static_cast<unsigned>(parsed);
    return true;
  } catch (const std::exception &) {
    return false;
  }
}

bool isValidEmail(const std::string &email) {
  const auto at = email.find('@');
  return at != std::string::npos && at != 0 && at + 1 < email.size() &&
         email.find('@', at + 1) == std::string::npos &&
         email.find_first_of(" \r\n\t") == std::string::npos;
}

bool isHttpUrl(const std::string &url) {
  return url.empty() ||
         ((url.rfind("https://", 0) == 0 ||
           url.rfind("http://", 0) == 0) &&
          url.find_first_of(" \r\n\t\"'<>") == std::string::npos);
}

void setConfigurationError(EmailConfig &config, const std::string &error) {
  if (config.configurationError.empty()) {
    config.configurationError = error;
  }
}
}  // namespace

EmailConfig EmailConfig::fromEnvironment() {
  EmailConfig config;
  const std::string enabled = environmentValue("SMTP_ENABLED");
  if (!enabled.empty() && !parseBoolean(enabled, config.enabled)) {
    setConfigurationError(config, "SMTP_ENABLED must be true or false");
  }
  if (const std::string host = environmentValue("SMTP_HOST"); !host.empty()) {
    config.host = host;
  }
  const std::string port = environmentValue("SMTP_PORT");
  if (!port.empty() && !parseUnsigned(port, config.port)) {
    setConfigurationError(config, "SMTP_PORT must be a positive integer");
  }
  const std::string security = environmentValue("SMTP_SECURITY");
  if (!security.empty()) {
    config.security = security;
  }
  config.username = environmentValue("SMTP_USERNAME");
  config.password = environmentValue("SMTP_PASSWORD");
  if (const std::string senderName = environmentValue("SMTP_SENDER_NAME");
      !senderName.empty()) {
    config.senderName = senderName;
  }
  config.fromEmail = environmentValue("SMTP_FROM_EMAIL");
  config.platformUrl = environmentValue("PLATFORM_URL");

  const std::string timeout = environmentValue("SMTP_TIMEOUT_SECONDS");
  if (!timeout.empty()) {
    unsigned parsedTimeout = 0;
    if (!parseUnsigned(timeout, parsedTimeout) || parsedTimeout == 0) {
      setConfigurationError(config,
                             "SMTP_TIMEOUT_SECONDS must be a positive integer");
    } else {
      config.timeoutSeconds = static_cast<long>(parsedTimeout);
    }
  }
  return config;
}

std::string EmailConfig::validationError() const {
  if (!configurationError.empty()) {
    return configurationError;
  }
  if (!enabled) {
    return "SMTP email notifications are disabled; set SMTP_ENABLED=true";
  }
  if (host.empty()) {
    return "SMTP_HOST is required";
  }
  if (port == 0 || port > 65535) {
    return "SMTP_PORT must be between 1 and 65535";
  }
  if (security != "starttls" && security != "tls") {
    return "SMTP_SECURITY must be starttls or tls; unencrypted SMTP is "
           "not allowed";
  }
  if (username.empty()) {
    return "SMTP_USERNAME is required";
  }
  if (password.empty()) {
    return "SMTP_PASSWORD is required";
  }
  if (!isValidEmail(fromEmail)) {
    return "SMTP_FROM_EMAIL must be a valid email address";
  }
  if (timeoutSeconds <= 0 || timeoutSeconds > 300) {
    return "SMTP_TIMEOUT_SECONDS must be between 1 and 300";
  }
  if (!isHttpUrl(platformUrl)) {
    return "PLATFORM_URL must be a safe http:// or https:// URL";
  }
  return {};
}
