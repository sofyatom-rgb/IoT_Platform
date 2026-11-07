#include "notifications/EmailTemplate.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <map>
#include <optional>
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

std::string currentMoscowTimestamp() {
  const std::time_t now =
      std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  constexpr std::time_t moscowOffsetSeconds = 3 * 60 * 60;
  const std::time_t moscowTime = now + moscowOffsetSeconds;
  std::tm localTime{};
  if (gmtime_r(&moscowTime, &localTime) == nullptr) {
    throw std::runtime_error("Could not format Moscow email timestamp");
  }
  std::ostringstream stream;
  stream << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S MSK (UTC+3)");
  return stream.str();
}

struct SeverityStyle {
  const char *label;
  const char *icon;
  const char *foreground;
  const char *background;
};

SeverityStyle severityStyle(const std::string &severity) {
  if (severity == "INFO") {
    return {"ИНФОРМАЦИЯ", "●", "#367d78", "#e6f4ef"};
  }
  if (severity == "WARNING") {
    return {"ВНИМАНИЕ", "▲", "#a85c42", "#fff0e8"};
  }
  if (severity == "CRITICAL") {
    return {"КРИТИЧНО", "◆", "#a84d63", "#fbeaf0"};
  }
  throw std::invalid_argument("severity must be INFO, WARNING, or CRITICAL");
}

std::optional<double> numericValue(const std::string &value) {
  try {
    std::size_t parsedLength = 0;
    const double parsed = std::stod(value, &parsedLength);
    if (parsedLength != value.size() || !std::isfinite(parsed)) {
      return std::nullopt;
    }
    return parsed;
  } catch (const std::invalid_argument &) {
    return std::nullopt;
  } catch (const std::out_of_range &) {
    return std::nullopt;
  }
}

std::string percentageWidth(double value, double maximum) {
  const double percentage =
      std::clamp(value / maximum * 100.0, 0.0, 100.0);
  return std::to_string(static_cast<int>(std::lround(percentage)));
}

std::string comparisonChart(const EmailAlertData &data) {
  const auto reading = numericValue(data.readingValue);
  const auto threshold = numericValue(data.threshold);
  if (!reading || !threshold) {
    return "<div style=\"padding:12px;color:#667085;font-size:13px;\">"
           "Сравнение недоступно: значение или норма не указаны.</div>";
  }

  const double maximum =
      std::max({1.0, *reading, *threshold}) * 1.15;
  const std::string normalWidth = percentageWidth(*threshold, maximum);
  const std::string readingWidth = percentageWidth(*reading, maximum);
  const std::string normalRemainder =
      std::to_string(100 - std::stoi(normalWidth));
  const std::string readingRemainder =
      std::to_string(100 - std::stoi(readingWidth));

  return "<table role=\"presentation\" width=\"100%\" cellspacing=\"0\" "
         "cellpadding=\"0\" border=\"0\" "
         "style=\"font-family:'Trebuchet MS',Arial,sans-serif;\">"
         "<tr><td style=\"padding:0 0 8px;color:#65706d;font-size:12px;\">"
         "Норма&nbsp; · &nbsp;" +
         EmailTemplate::escapeHtml(valueOrDash(data.threshold)) + " " +
         EmailTemplate::escapeHtml(valueOrDash(data.unit)) +
         "</td></tr><tr><td style=\"padding:0 0 15px;\">"
         "<table role=\"presentation\" width=\"100%\" cellspacing=\"0\" "
         "cellpadding=\"0\" border=\"0\" style=\"height:10px;"
         "background-color:#edf0eb;border-radius:8px;\">"
         "<tr><td width=\"" +
         normalWidth +
         "%\" style=\"height:10px;background-color:#a9d8c8;"
         "border-radius:8px 0 0 8px;font-size:0;line-height:0;\">&nbsp;"
         "</td><td width=\"" +
         normalRemainder +
         "%\" style=\"height:10px;font-size:0;line-height:0;\">&nbsp;"
         "</td></tr></table></td></tr>"
         "<tr><td style=\"padding:0 0 8px;color:#65706d;font-size:12px;\">"
         "Показатель&nbsp; · &nbsp;" +
         EmailTemplate::escapeHtml(valueOrDash(data.readingValue)) + " " +
         EmailTemplate::escapeHtml(valueOrDash(data.unit)) +
         "</td></tr><tr><td>"
         "<table role=\"presentation\" width=\"100%\" cellspacing=\"0\" "
         "cellpadding=\"0\" border=\"0\" style=\"height:10px;"
         "background-color:#edf0eb;border-radius:8px;\">"
         "<tr><td width=\"" +
         readingWidth +
         "%\" style=\"height:10px;background-color:#e9a98e;"
         "border-radius:8px 0 0 8px;font-size:0;line-height:0;\">&nbsp;"
         "</td><td width=\"" +
         readingRemainder +
         "%\" style=\"height:10px;font-size:0;line-height:0;\">&nbsp;"
         "</td></tr></table></td></tr></table>";
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
      {"occurred_at",
       escape(data.occurredAt.empty() ? currentMoscowTimestamp()
                                      : data.occurredAt)},
      {"severity", style.label},
      {"severity_icon", style.icon},
      {"severity_foreground", style.foreground},
      {"severity_background", style.background},
      {"comparison_chart",
       escapeValues ? comparisonChart(data) : std::string{}},
      {"description", escape(valueOrDash(data.description))},
      {"platform_link",
       isSafeHttpUrl(platformUrl)
           ? "Открыть панель управления: " + escape(platformUrl)
           : ""},
      {"platform_button",
       isSafeHttpUrl(platformUrl)
           ? "<tr><td align=\"center\" style=\"padding:0 24px 28px;\">"
             "<a href=\"" +
                 EmailTemplate::escapeHtml(platformUrl) +
                 "\" style=\"display:inline-block;background:#5151c6;"
                 "color:#ffffff;text-decoration:none;padding:12px 24px;"
                 "border-radius:6px;font-family:Arial,sans-serif;"
                 "font-size:15px;font-weight:bold;\">Открыть панель управления</a>"
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
  return {"[" + safeSubjectText(severityStyle(data.severity).label) + "] " +
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
