#include "notifications/TelegramMessageTemplates.h"

#include <cctype>
#include <fstream>
#include <set>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace {
const std::set<std::string> &supportedVariables() {
  static const std::set<std::string> variables = {
      "description",   "device_id",     "device_name",
      "event_type",    "excess",        "location",
      "message",       "occurred_at",   "notification_time",
      "platform_name", "reading_value", "recipient_name",
      "sensor_id",     "sensor_name",   "severity",
      "temperature",   "threshold",     "unit"};
  return variables;
}

bool isIdentifier(const std::string &value) {
  if (value.empty() ||
      !(std::isalpha(static_cast<unsigned char>(value.front())) ||
        value.front() == '_')) {
    return false;
  }
  for (const unsigned char character : value) {
    if (!std::isalnum(character) && character != '_') {
      return false;
    }
  }
  return true;
}

std::string validateParseMode(const Json::Value &value,
                              const std::string &context) {
  if (!value.isString()) {
    throw std::runtime_error(context + " parse_mode must be a string");
  }
  const std::string parseMode = value.asString();
  if (parseMode != "HTML" && parseMode != "MarkdownV2") {
    throw std::runtime_error(context +
                             " parse_mode must be HTML or MarkdownV2");
  }
  return parseMode;
}

void validateTemplateText(const std::string &name, const std::string &text) {
  if (text.empty()) {
    throw std::runtime_error("Telegram template '" + name + "' is empty");
  }

  std::size_t position = 0;
  while (position < text.size()) {
    const auto opening = text.find("{{", position);
    const auto strayClosing = text.find("}}", position);
    if (strayClosing != std::string::npos &&
        (opening == std::string::npos || strayClosing < opening)) {
      throw std::runtime_error("Telegram template '" + name +
                               "' contains an unmatched '}}'");
    }
    if (opening == std::string::npos) {
      break;
    }
    const auto closing = text.find("}}", opening + 2);
    if (closing == std::string::npos) {
      throw std::runtime_error("Telegram template '" + name +
                               "' contains an unmatched '{{'");
    }
    const std::string variable =
        text.substr(opening + 2, closing - opening - 2);
    if (!isIdentifier(variable)) {
      throw std::runtime_error("Invalid variable name in Telegram template '" +
                               name + "'");
    }
    if (!supportedVariables().contains(variable)) {
      throw std::runtime_error("Unknown variable '{{" + variable +
                               "}}' in Telegram template '" + name + "'");
    }
    position = closing + 2;
  }
}

std::string readTemplateText(const std::string &name,
                             const Json::Value &value) {
  if (value.isString()) {
    return value.asString();
  }
  if (!value.isArray() || value.empty()) {
    throw std::runtime_error("Telegram template '" + name +
                             "' text must be a string or a non-empty array "
                             "of lines");
  }

  std::string text;
  for (Json::ArrayIndex index = 0; index < value.size(); ++index) {
    if (!value[index].isString()) {
      throw std::runtime_error("Telegram template '" + name +
                               "' text lines must be strings");
    }
    if (index != 0) {
      text += '\n';
    }
    text += value[index].asString();
  }
  return text;
}

std::string escapeHtml(const std::string &value) {
  std::string result;
  result.reserve(value.size());
  for (const char character : value) {
    switch (character) {
    case '&':
      result += "&amp;";
      break;
    case '<':
      result += "&lt;";
      break;
    case '>':
      result += "&gt;";
      break;
    case '"':
      result += "&quot;";
      break;
    case '\'':
      result += "&#39;";
      break;
    default:
      result += character;
      break;
    }
  }
  return result;
}

std::string escapeMarkdownV2(const std::string &value) {
  constexpr std::string_view reserved = "_*[]()~`>#+-=|{}.!\\";
  std::string result;
  result.reserve(value.size() * 2);
  for (const char character : value) {
    if (reserved.find(character) != std::string_view::npos) {
      result += '\\';
    }
    result += character;
  }
  return result;
}

std::string escapeValue(const std::string &value,
                        const std::string &parseMode) {
  return parseMode == "HTML" ? escapeHtml(value) : escapeMarkdownV2(value);
}

Json::Value defaultConfig() {
  Json::Value config;
  config["version"] = 1;
  config["defaults"]["parse_mode"] = "HTML";
  config["templates"]["temperature_threshold"]["text"] =
      "🚨 <b>ТЕМПЕРАТУРА ВЫШЕ ПОРОГА</b>\n\n"
      "📟 Устройство: <b>{{device_name}}</b>\n"
      "🌡 Сейчас: <b>{{temperature}} {{unit}}</b>\n"
      "🎯 Порог: {{threshold}} {{unit}}\n"
      "📈 Превышение: <b>{{excess}} {{unit}}</b>\n"
      "🆔 ID устройства: <b>{{device_id}}</b>\n"
      "⚡ Уровень: {{severity}}\n"
      "🕒 Время уведомления: {{notification_time}}";
  return config;
}
} // namespace

TelegramMessageTemplates::TelegramMessageTemplates()
    : TelegramMessageTemplates(defaultConfig()) {}

TelegramMessageTemplates::TelegramMessageTemplates(
    const std::string &configPath) {
  std::ifstream input(configPath);
  if (!input) {
    throw std::runtime_error("Could not open Telegram templates config: " +
                             configPath);
  }

  Json::CharReaderBuilder builder;
  Json::Value config;
  std::string errors;
  if (!Json::parseFromStream(builder, input, &config, &errors)) {
    throw std::runtime_error("Could not parse Telegram templates config " +
                             configPath + ": " + errors);
  }

  *this = TelegramMessageTemplates(config);
}

TelegramMessageTemplates::TelegramMessageTemplates(const Json::Value &config) {
  if (!config["version"].isInt() || config["version"].asInt() != 1) {
    throw std::runtime_error("Telegram templates config version must be 1");
  }
  if (!config["defaults"].isObject()) {
    throw std::runtime_error("Telegram templates config requires defaults");
  }
  const std::string defaultParseMode =
      validateParseMode(config["defaults"]["parse_mode"], "defaults");
  if (!config["templates"].isObject() || config["templates"].empty()) {
    throw std::runtime_error("Telegram templates config requires templates");
  }

  for (const auto &name : config["templates"].getMemberNames()) {
    if (!isIdentifier(name)) {
      throw std::runtime_error("Invalid Telegram template name: " + name);
    }
    const auto &entry = config["templates"][name];
    if (!entry.isObject() ||
        (!entry["text"].isString() && !entry["text"].isArray())) {
      throw std::runtime_error("Telegram template '" + name +
                               "' requires text as a string or line array");
    }
    const std::string text = readTemplateText(name, entry["text"]);
    validateTemplateText(name, text);
    const std::string parseMode =
        entry.isMember("parse_mode")
            ? validateParseMode(entry["parse_mode"], "template '" + name + "'")
            : defaultParseMode;
    templates_.emplace(name, Template{text, parseMode});
  }
}

TelegramRenderedMessage TelegramMessageTemplates::render(
    const std::string &templateName,
    const std::map<std::string, std::string> &values) const {
  const auto found = templates_.find(templateName);
  if (found == templates_.end()) {
    throw std::runtime_error("Telegram template not found: " + templateName);
  }

  std::string text = found->second.text;
  std::size_t position = 0;
  while ((position = text.find("{{", position)) != std::string::npos) {
    const auto closing = text.find("}}", position + 2);
    if (closing == std::string::npos) {
      throw std::runtime_error("Malformed Telegram template: " + templateName);
    }
    const std::string variable =
        text.substr(position + 2, closing - position - 2);
    const auto value = values.find(variable);
    if (value == values.end()) {
      throw std::runtime_error(
          "No value provided for Telegram template variable '" + variable +
          "'");
    }
    const std::string escaped =
        escapeValue(value->second, found->second.parseMode);
    text.replace(position, closing + 2 - position, escaped);
    position += escaped.size();
  }

  return {std::move(text), found->second.parseMode};
}
