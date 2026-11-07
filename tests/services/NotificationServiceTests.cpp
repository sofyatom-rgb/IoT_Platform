#include "TestFramework.h"

#include "services/NotificationService.h"

#include <memory>
#include <stdexcept>
#include <utility>

namespace {
class FakeTelegramClient final : public ITelegramBotClient {
public:
  mutable std::string lastChatId, lastText, lastParseMode, failure;
  mutable int calls{0};

  void
  sendMessage(const std::string &chatId, const std::string &text,
              std::function<void()> success,
              std::function<void(const std::string &)> error) const override {
    lastChatId = chatId;
    lastText = text;
    ++calls;
    if (failure.empty())
      success();
    else
      error(failure);
  }

  void
  sendMessage(const std::string &chatId, const std::string &text,
              const std::string &parseMode, std::function<void()> success,
              std::function<void(const std::string &)> error) const override {
    lastParseMode = parseMode;
    sendMessage(chatId, text, std::move(success), std::move(error));
  }
};
} // namespace

TEST_CASE(notification_service_formats_temperature_alert) {
  auto client = std::make_shared<FakeTelegramClient>();
  NotificationService service(client);
  bool sent = false;
  service.sendTemperatureAlert(
      "chat-42", "Living room", 31.5, 28.0, [&] { sent = true; },
      [](const std::string &) { EXPECT_TRUE(false); });
  EXPECT_TRUE(sent);
  EXPECT_EQ("chat-42", client->lastChatId);
  EXPECT_EQ("HTML", client->lastParseMode);
  EXPECT_TRUE(client->lastText.find("<b>ТЕМПЕРАТУРА ВЫШЕ ПОРОГА</b>") !=
              std::string::npos);
  EXPECT_TRUE(client->lastText.find("Устройство: <b>Living room</b>") !=
              std::string::npos);
  EXPECT_TRUE(client->lastText.find("Сейчас: <b>31.5 °C</b>") !=
              std::string::npos);
  EXPECT_TRUE(client->lastText.find("Превышение: <b>3.5 °C</b>") !=
              std::string::npos);
  EXPECT_TRUE(client->lastText.find("Время уведомления: ") !=
              std::string::npos);
  EXPECT_TRUE(client->lastText.find("MSK (UTC+3)") != std::string::npos);
  EXPECT_EQ(1, client->calls);
}

TEST_CASE(notification_service_escapes_template_values) {
  auto client = std::make_shared<FakeTelegramClient>();
  NotificationService service(client);
  service.sendTemperatureAlert(
      "chat-42", "<Living_room & lab>", 31.5, 28.0, [] {},
      [](const std::string &) { EXPECT_TRUE(false); });

  EXPECT_TRUE(client->lastText.find(
                  "Устройство: <b>&lt;Living_room &amp; lab&gt;</b>") !=
              std::string::npos);
}

TEST_CASE(notification_service_propagates_telegram_error) {
  auto client = std::make_shared<FakeTelegramClient>();
  client->failure = "Telegram is unavailable";
  NotificationService service(client);
  std::string observedError;
  service.sendTemperatureAlert(
      "chat-1", "Sensor", 40.0, 30.0, [] { EXPECT_TRUE(false); },
      [&](const std::string &error) { observedError = error; });
  EXPECT_EQ("Telegram is unavailable", observedError);
}

TEST_CASE(telegram_templates_allow_per_message_modes_and_variables) {
  Json::Value config;
  config["version"] = 1;
  config["defaults"]["parse_mode"] = "HTML";
  config["templates"]["generic_event"]["text"] =
      "<b>{{event_type}}</b>: {{message}}";
  config["templates"]["compact"]["parse_mode"] = "MarkdownV2";
  config["templates"]["compact"]["text"] = "*{{event_type}}* {{device_name}}";
  TelegramMessageTemplates templates(config);

  const auto html =
      templates.render("generic_event", {{"event_type", "Sensor <offline>"},
                                         {"message", "Room & sensor"}});
  EXPECT_EQ("HTML", html.parseMode);
  EXPECT_EQ("<b>Sensor &lt;offline&gt;</b>: Room &amp; sensor", html.text);

  const auto markdown = templates.render(
      "compact", {{"event_type", "Alarm!"}, {"device_name", "Room_1"}});
  EXPECT_EQ("MarkdownV2", markdown.parseMode);
  EXPECT_EQ("*Alarm\\!* Room\\_1", markdown.text);
}

TEST_CASE(telegram_templates_load_configured_temperature_message) {
  TelegramMessageTemplates templates(
      std::string(TELEGRAM_TEMPLATES_CONFIG_PATH));
  const auto rendered = templates.render(
      "temperature_threshold",
      {{"device_id", "42"},
       {"device_name", "Living room"},
       {"event_type", "Превышен порог температуры"},
       {"excess", "11"},
       {"location", "—"},
       {"message", "Перегрев"},
       {"notification_time", "2026-11-07 19:00:00 MSK (UTC+3)"},
       {"occurred_at", "2026-11-07 16:00:00 UTC"},
       {"platform_name", "IoT Monitor"},
       {"reading_value", "51"},
       {"recipient_name", ""},
       {"sensor_id", "42"},
       {"sensor_name", "Living room"},
       {"severity", "WARNING"},
       {"temperature", "51"},
       {"threshold", "40"},
       {"unit", "°C"},
       {"description", "Перегрев"}});

  EXPECT_EQ("HTML", rendered.parseMode);
  EXPECT_TRUE(rendered.text.find("<b>51 °C</b>") != std::string::npos);
  EXPECT_TRUE(rendered.text.find("<b>42</b>") != std::string::npos);
  EXPECT_TRUE(rendered.text.find("<b>11 °C</b>") != std::string::npos);
}

TEST_CASE(telegram_templates_reject_unknown_variables_at_load_time) {
  Json::Value config;
  config["version"] = 1;
  config["defaults"]["parse_mode"] = "HTML";
  config["templates"]["invalid"]["text"] = "Unknown {{temperature_celsius}}";

  bool rejected = false;
  try {
    TelegramMessageTemplates templates(config);
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  EXPECT_TRUE(rejected);
}
