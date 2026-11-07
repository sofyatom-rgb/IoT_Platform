#include "notifications/EmailConfig.h"
#include "notifications/EmailService.h"
#include "notifications/EmailTemplate.h"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
class EnvironmentGuard {
 public:
  explicit EnvironmentGuard(std::vector<std::string> variables) {
    previous_.reserve(variables.size());
    for (const auto &name : variables) {
      const char *value = std::getenv(name.c_str());
      previous_.emplace_back(name, value == nullptr
                                       ? std::optional<std::string>{}
                                       : std::optional<std::string>{value});
    }
  }

  ~EnvironmentGuard() {
    for (const auto &[name, value] : previous_) {
      if (value) {
        setenv(name.c_str(), value->c_str(), 1);
      } else {
        unsetenv(name.c_str());
      }
    }
  }

 private:
  std::vector<std::pair<std::string, std::optional<std::string>>> previous_;
};

const std::vector<std::string> smtpVariables = {
    "SMTP_ENABLED", "SMTP_HOST", "SMTP_PORT", "SMTP_SECURITY",
    "SMTP_USERNAME", "SMTP_PASSWORD", "SMTP_FROM_EMAIL",
    "SMTP_SENDER_NAME", "SMTP_TIMEOUT_SECONDS", "PLATFORM_URL"};

void require(bool condition, const std::string &message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void testEnvironmentDefaultsToDisabled() {
  const EnvironmentGuard guard(smtpVariables);
  for (const auto &name : smtpVariables) {
    unsetenv(name.c_str());
  }

  const EmailConfig config = EmailConfig::fromEnvironment();
  require(!config.enabled, "SMTP must be disabled by default");
  require(config.host == "smtp.gmail.com", "Gmail host default is missing");
  require(config.port == 587, "Gmail STARTTLS port default is missing");
  require(config.security == "starttls",
          "Gmail STARTTLS security default is missing");
}

void testEnvironmentConfigurationAndValidation() {
  const EnvironmentGuard guard(smtpVariables);
  setenv("SMTP_ENABLED", "true", 1);
  setenv("SMTP_HOST", "smtp.example.test", 1);
  setenv("SMTP_PORT", "587", 1);
  setenv("SMTP_SECURITY", "starttls", 1);
  setenv("SMTP_USERNAME", "sender@example.test", 1);
  setenv("SMTP_PASSWORD", "unit-test-secret", 1);
  setenv("SMTP_FROM_EMAIL", "sender@example.test", 1);
  setenv("SMTP_SENDER_NAME", "IoT Monitor", 1);
  setenv("SMTP_TIMEOUT_SECONDS", "10", 1);
  setenv("PLATFORM_URL", "https://monitor.example.test", 1);

  const EmailConfig config = EmailConfig::fromEnvironment();
  require(config.enabled, "SMTP_ENABLED was not read");
  require(config.host == "smtp.example.test", "SMTP_HOST was not read");
  require(config.security == "starttls", "SMTP_SECURITY was not read");
  require(config.username == "sender@example.test",
          "SMTP_USERNAME was not read");
  require(config.password == "unit-test-secret",
          "SMTP_PASSWORD was not read");
  require(config.validationError().empty(),
          "Valid SMTP environment was rejected");

  setenv("SMTP_SECURITY", "none", 1);
  const EmailConfig insecureConfig = EmailConfig::fromEnvironment();
  require(!insecureConfig.validationError().empty(),
          "Unencrypted SMTP to a non-loopback host was accepted");
  setenv("SMTP_HOST", "127.0.0.1", 1);
  setenv("SMTP_USERNAME", "", 1);
  setenv("SMTP_PASSWORD", "", 1);
  const EmailConfig localSmtpConfig = EmailConfig::fromEnvironment();
  require(localSmtpConfig.validationError().empty(),
          "Local unauthenticated SMTP configuration was rejected");
  setenv("SMTP_HOST", "smtp.example.test", 1);
  setenv("SMTP_USERNAME", "sender@example.test", 1);
  setenv("SMTP_PASSWORD", "unit-test-secret", 1);
  setenv("SMTP_SECURITY", "starttls", 1);
  setenv("PLATFORM_URL", "https://monitor.example.test/\" onclick=\"alert(1)",
         1);
  const EmailConfig unsafeUrlConfig = EmailConfig::fromEnvironment();
  require(!unsafeUrlConfig.validationError().empty(),
          "Unsafe platform URL was accepted");
}

void testTemplatesEscapeDataAndRenderFields() {
  const EmailTemplate emailTemplate;
  EmailAlertData alert;
  alert.recipientName = "Alex <admin>";
  alert.platformName = "IoT Monitor";
  alert.eventType = "Temperature alert";
  alert.sensorName = "Sensor <script>";
  alert.sensorId = "sensor-42";
  alert.readingValue = "38.7";
  alert.unit = "°C";
  alert.threshold = "30";
  alert.location = "Room & lab";
  alert.occurredAt = "2026-11-07 12:00 UTC";
  alert.severity = "CRITICAL";
  alert.description = "Check <device> & reset.";

  const RenderedEmail rendered =
      emailTemplate.render(alert, "https://monitor.example.test/path?a=1&b=2");
  require(rendered.html.find("Sensor &lt;script&gt;") != std::string::npos,
          "HTML sensor name was not escaped");
  require(rendered.html.find("<script>") == std::string::npos,
          "HTML contains an unescaped script tag");
  require(rendered.html.find("Alex &lt;admin&gt;") != std::string::npos,
          "HTML recipient name was not escaped");
  require(rendered.html.find("Room &amp; lab") != std::string::npos,
          "HTML location was not escaped");
  require(rendered.html.find("Дата и время") != std::string::npos &&
              rendered.html.find("2026-11-07 12:00 UTC") != std::string::npos,
          "HTML email did not include the event date and time");
  require(rendered.html.find("https://monitor.example.test/path?a=1&amp;b=2") !=
              std::string::npos,
          "Platform URL was not safely escaped");
  require(rendered.plainText.find(
              "Открыть панель управления: "
              "https://monitor.example.test/path?a=1&b=2") !=
              std::string::npos,
          "Plain-text email did not include the configured platform link");
  require(rendered.plainText.find("Sensor <script>") != std::string::npos,
          "Plain-text template did not include data");
  require(rendered.plainText.find("Дата и время: 2026-11-07 12:00 UTC") !=
              std::string::npos,
          "Plain-text email did not include the event date and time");
  require(rendered.subject.find("[КРИТИЧНО]") == 0,
          "Email subject did not include Russian severity");
  alert.eventType = "Temperature alert\r\nBcc: attacker@example.test";
  require(emailTemplate.render(alert, {}).subject.find('\n') ==
              std::string::npos,
          "Email subject allowed a line break");
}

void testSeverityStylesAndOptionalPlatformLink() {
  const EmailTemplate emailTemplate;
  EmailAlertData alert;
  alert.eventType = "Alert";
  alert.sensorName = "Sensor";
  alert.readingValue = "38.7";
  alert.threshold = "30";
  alert.severity = "INFO";
  const std::string info = emailTemplate.renderHtml(alert, {});
  require(info.find("#e6f4ef") != std::string::npos &&
              info.find("'Trebuchet MS'") != std::string::npos,
          "Pastel palette or rounded sans-serif font is missing");
  require(info.find("ТЕКУЩЕЕ ПОКАЗАНИЕ") != std::string::npos &&
              info.find("СРАВНЕНИЕ С НОРМОЙ") != std::string::npos &&
              info.find("ДАТА И ВРЕМЯ") != std::string::npos,
          "Email is missing Russian measurement details");
  require(info.find("width=\"67%\"") != std::string::npos &&
              info.find("width=\"87%\"") != std::string::npos,
          "Comparison chart does not scale bars to the threshold and reading");
  require(info.find("MSK (UTC+3)") != std::string::npos,
          "Missing occurred_at did not fall back to a Moscow timestamp");

  alert.severity = "WARNING";
  const std::string warning = emailTemplate.renderHtml(alert, {});
  require(warning.find("#fff0e8") != std::string::npos &&
              warning.find("ВНИМАНИЕ") != std::string::npos,
          "WARNING severity style is missing");

  alert.severity = "CRITICAL";
  const std::string critical = emailTemplate.renderHtml(alert, {});
  require(critical.find("#fbeaf0") != std::string::npos &&
              critical.find("КРИТИЧНО") != std::string::npos,
          "CRITICAL severity style is missing");

  require(critical.find("Открыть панель управления") == std::string::npos,
          "Platform link should be omitted when URL is not configured");
  bool invalidSeverityRejected = false;
  alert.severity = "EMERGENCY";
  try {
    emailTemplate.renderHtml(alert, {});
  } catch (const std::invalid_argument &) {
    invalidSeverityRejected = true;
  }
  require(invalidSeverityRejected, "Unknown severity was not rejected");
}

void testMissingConfigurationAndSendErrors() {
  EmailConfig config;
  EmailService service(config);
  const EmailSendResult result = service.sendTestEmail("user@example.test");
  require(!result.sent, "Email sent without SMTP being enabled");
  require(result.error.find("SMTP email notifications are disabled") !=
              std::string::npos,
          "Disabled SMTP error is unclear");

  config.enabled = true;
  EmailService incompleteService(config);
  const EmailSendResult incomplete =
      incompleteService.sendTestEmail("user@example.test");
  require(!incomplete.sent, "Email sent with incomplete SMTP configuration");
  require(incomplete.error.find("SMTP_USERNAME is required") !=
              std::string::npos,
          "Missing SMTP credentials error is unclear");
}
}  // namespace

int main() {
  try {
    testEnvironmentDefaultsToDisabled();
    testEnvironmentConfigurationAndValidation();
    testTemplatesEscapeDataAndRenderFields();
    testSeverityStylesAndOptionalPlatformLink();
    testMissingConfigurationAndSendErrors();
  } catch (const std::exception &exception) {
    std::cerr << exception.what() << '\n';
    return 1;
  }
  std::cout << "All notification service tests passed\n";
  return 0;
}
