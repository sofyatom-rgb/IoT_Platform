#include "TestFramework.h"

#include "services/NotificationService.h"

#include <memory>

namespace {
class FakeTelegramClient final : public ITelegramBotClient {
 public:
  mutable std::string lastChatId, lastText, failure;
  mutable int calls{0};

  void sendMessage(const std::string &chatId, const std::string &text,
                   std::function<void()> success,
                   std::function<void(const std::string &)> error) const override {
    lastChatId = chatId; lastText = text; ++calls;
    if (failure.empty()) success(); else error(failure);
  }
};
}  // namespace

TEST_CASE(notification_service_formats_temperature_alert) {
  auto client = std::make_shared<FakeTelegramClient>();
  NotificationService service(client);
  bool sent = false;
  service.sendTemperatureAlert("chat-42", "Living room", 31.5, 28.0,
                               [&] { sent = true; },
                               [](const std::string &) { EXPECT_TRUE(false); });
  EXPECT_TRUE(sent);
  EXPECT_EQ("chat-42", client->lastChatId);
  const std::string expected =
      "Внимание! На устройстве \"Living room\" температура " +
      std::to_string(31.5) + " °C превысила порог " +
      std::to_string(28.0) + " °C.";
  EXPECT_EQ(expected, client->lastText);
  EXPECT_EQ(1, client->calls);
}

TEST_CASE(notification_service_propagates_telegram_error) {
  auto client = std::make_shared<FakeTelegramClient>();
  client->failure = "Telegram is unavailable";
  NotificationService service(client);
  std::string observedError;
  service.sendTemperatureAlert("chat-1", "Sensor", 40.0, 30.0,
                               [] { EXPECT_TRUE(false); },
                               [&](const std::string &error) { observedError = error; });
  EXPECT_EQ("Telegram is unavailable", observedError);
}
