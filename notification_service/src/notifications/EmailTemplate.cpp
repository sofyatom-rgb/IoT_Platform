#include "notifications/EmailTemplate.h"

#include <cctype>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

#ifndef EMAIL_TEMPLATE_DIR
#define EMAIL_TEMPLATE_DIR "resources/email"
#endif

namespace {
std::string readTemplate(const std::string &path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw std::runtime_error("Could not read email template: " + path);
  }
  std::ostringstream content;
  content << input.rdbuf();
  if (!input.good() && !input.eof()) {
    throw std::runtime_error("Could not read email template: " + path);
  }
  return content.str();
}

std::string replaceAll(std::string text, const std::string &placeholder,
                       const std::string &value) {
  std::size_t position = 0;
  while ((position = text.find(placeholder, position)) != std::string::npos) {
    text.replace(position, placeholder.size(), value);
    position += value.size();
  }
  return text;
}

std::string valueOrDash(const std::string &value) {
  return value.empty() ? "—" : value;
}

struct SeverityStyle {
  const char *label;
  const char *icon;
  const char *foreground;
  const char *background;
};

SeverityStyle severityStyle(const std::string &severity) {
  if (severity == "INFO") {
    return {"INFO", "●", "#67e8f9", "#102a3a"};
  }
  if (severity == "WARNING") {
    return {"WARNING", "▲", "#fbbf24", "#3b2b12"};
  }
  if (severity == "CRITICAL") {
    return {"CRITICAL", "◆", "#fb7185", "#3b1827"};
  }
  throw std::invalid_argument("severity must be INFO, WARNING, or CRITICAL");
}

bool isSafeHttpUrl(const std::string &url) {
  return (url.rfind("https://", 0) == 0 || url.rfind("http://", 0) == 0) &&
         url.find_first_of(" \r\n\t\"'<>") == std::string::npos;
}

std::string safeSubjectText(const std::string &value) {
  std::string result;
  result.reserve(value.size());
  for (const unsigned char character : value) {
    if (std::iscntrl(character) != 0) {
      if (character == '\r' || character == '\n' || character == '\t') {
        result.push_back(' ');
      }
      continue;
    }
    result.push_back(static_cast<char>(character));
  }
  return result;
}

std::map<std::string, std::string> templateValues(
    const EmailAlertData &data, const std::string &platformUrl,
    bool escapeValues) {
  const SeverityStyle style = severityStyle(data.severity);
  const auto escape = [escapeValues](const std::string &value) {
    return escapeValues ? EmailTemplate::escapeHtml(value) : value;
  };
  std::map<std::string, std::string> values = {
      {"platform_name",
       escape(data.platformName.empty() ? "IoT Monitor" : data.platformName)},
      {"recipient_greeting",
       data.recipientName.empty()
           ? "Здравствуйте!"
           : "Здравствуйте, " + escape(data.recipientName) + "!"},
      {"event_type", escape(valueOrDash(data.eventType))},
      {"sensor_name", escape(valueOrDash(data.sensorName))},
      {"sensor_id", escape(valueOrDash(data.sensorId))},
      {"reading_value", escape(valueOrDash(data.readingValue))},
      {"unit", escape(valueOrDash(data.unit))},
      {"threshold", escape(valueOrDash(data.threshold))},
      {"location", escape(valueOrDash(data.location))},
      {"occurred_at", escape(valueOrDash(data.occurredAt))},
      {"severity", style.label},
      {"severity_icon", style.icon},
      {"severity_foreground", style.foreground},
      {"severity_background", style.background},
      {"description", escape(valueOrDash(data.description))},
      {"platform_link",
       isSafeHttpUrl(platformUrl)
           ? "Открыть IoT Monitor: " + escape(platformUrl)
           : ""},
      {"platform_button",
       isSafeHttpUrl(platformUrl)
           ? "<tr><td align=\"center\" style=\"padding:0 24px 28px;\">"
             "<a href=\"" +
                 EmailTemplate::escapeHtml(platformUrl) +
                 "\" style=\"display:inline-block;background:#2563eb;"
                 "color:#ffffff;text-decoration:none;padding:12px 24px;"
                 "border-radius:6px;font-family:Arial,sans-serif;"
                 "font-size:15px;font-weight:bold;\">Открыть IoT Monitor</a>"
                 "</td></tr>"
           : ""}};
  return values;
}

std::string applyValues(
    std::string text,
    const std::map<std::string, std::string> &values) {
  for (const auto &[key, value] : values) {
    text = replaceAll(std::move(text), "{{" + key + "}}", value);
  }
  return text;
}
}  // namespace

EmailTemplate::EmailTemplate(std::string templateDirectory) {
  if (templateDirectory.empty()) {
    templateDirectory = EMAIL_TEMPLATE_DIR;
  }
  if (!templateDirectory.empty() && templateDirectory.back() != '/') {
    templateDirectory.push_back('/');
  }
  htmlTemplate_ = readTemplate(templateDirectory + "alert.html");
  textTemplate_ = readTemplate(templateDirectory + "alert.txt");
}

RenderedEmail EmailTemplate::render(const EmailAlertData &data,
                                    const std::string &platformUrl) const {
  const std::string sensor =
      data.sensorName.empty() ? "датчик" : data.sensorName;
  return {"[" + safeSubjectText(data.severity) + "] " +
              safeSubjectText(data.eventType.empty() ? "Уведомление"
                                                     : data.eventType) +
                  ": " + safeSubjectText(sensor),
              renderPlainText(data, platformUrl), renderHtml(data, platformUrl)};
}

std::string EmailTemplate::renderHtml(const EmailAlertData &data,
                                      const std::string &platformUrl) const {
  return applyValues(htmlTemplate_,
                     templateValues(data, platformUrl, true));
}

std::string EmailTemplate::renderPlainText(
    const EmailAlertData &data, const std::string &platformUrl) const {
  auto values = templateValues(data, platformUrl, false);
  return applyValues(textTemplate_, values);
}

std::string EmailTemplate::escapeHtml(const std::string &value) {
  std::string escaped;
  escaped.reserve(value.size());
  for (const char character : value) {
    switch (character) {
      case '&':
        escaped += "&amp;";
        break;
      case '<':
        escaped += "&lt;";
        break;
      case '>':
        escaped += "&gt;";
        break;
      case '"':
        escaped += "&quot;";
        break;
      case '\'':
        escaped += "&#39;";
        break;
      default:
        escaped += character;
        break;
    }
  }
  return escaped;
}
