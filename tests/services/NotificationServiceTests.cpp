#include "TestFramework.h"
#include "TestSupport.h"

#include <fakeit.hpp>
#include "services/NotificationService.h"

#include <memory>
#include <string>

/// Проверяет текст уведомления и параметры вызова Telegram-клиента.
TEST_CASE(notification_service_formats_temperature_alert) {
  auto clientMock = std::make_shared<fakeit::Mock<ITelegramBotClient>>();
  auto client = test::sharedMock(clientMock);
  std::string receivedChatId, receivedText;
  fakeit::When(Method(*clientMock, sendMessage))
      .Do([&](const std::string &chatId, const std::string &text,
              std::function<void()> success,
              std::function<void(const std::string &)>) {
        receivedChatId = chatId;
        receivedText = text;
        success();
      });

  NotificationService service(client);
  bool sent = false;
  service.sendTemperatureAlert("chat-42", "Living room", 31.5, 28.0,
                               [&] { sent = true; },
                               [](const std::string &) { EXPECT_TRUE(false); });
  EXPECT_TRUE(sent);
  EXPECT_EQ("chat-42", receivedChatId);
  const std::string expected =
      "Внимание! На устройстве \"Living room\" температура " +
      std::to_string(31.5) + " °C превысила порог " +
      std::to_string(28.0) + " °C.";
  EXPECT_EQ(expected, receivedText);
  fakeit::Verify(Method(*clientMock, sendMessage)).Once();
}

/// Проверяет передачу ошибки Telegram вызывающему коду.
TEST_CASE(notification_service_propagates_telegram_error) {
  auto clientMock = std::make_shared<fakeit::Mock<ITelegramBotClient>>();
  auto client = test::sharedMock(clientMock);
  fakeit::When(Method(*clientMock, sendMessage))
      .Do([](const std::string &, const std::string &, std::function<void()>,
             std::function<void(const std::string &)> error) {
        error("Telegram is unavailable");
      });

  NotificationService service(client);
  std::string observedError;
  service.sendTemperatureAlert("chat-1", "Sensor", 40.0, 30.0,
                               [] { EXPECT_TRUE(false); },
                               [&](const std::string &error) { observedError = error; });
  EXPECT_EQ("Telegram is unavailable", observedError);
  fakeit::Verify(Method(*clientMock, sendMessage)).Once();
}
