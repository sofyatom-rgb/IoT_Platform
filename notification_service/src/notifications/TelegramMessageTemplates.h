#pragma once

#include <map>
#include <string>

#include <json/json.h>

struct TelegramRenderedMessage {
  std::string text;
  std::string parseMode;
};

class TelegramMessageTemplates {
public:
  TelegramMessageTemplates();
  explicit TelegramMessageTemplates(const std::string &configPath);
  explicit TelegramMessageTemplates(const Json::Value &config);

  TelegramRenderedMessage
  render(const std::string &templateName,
         const std::map<std::string, std::string> &values) const;

private:
  struct Template {
    std::string text;
    std::string parseMode;
  };

  std::map<std::string, Template> templates_;
};
