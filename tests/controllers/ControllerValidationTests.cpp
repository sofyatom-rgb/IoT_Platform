#include "TestFramework.h"
#include "common/validation/RequestValidation.h"

#include <limits>
#include <string>

TEST_CASE(controller_id_parser_accepts_positive_decimal_ids) {
  EXPECT_EQ(true, iot::validation::parsePositiveId("1").has_value());
  EXPECT_EQ(true, iot::validation::parsePositiveId("42").has_value());
  EXPECT_EQ(true, iot::validation::parsePositiveId("2147483647").has_value());
  EXPECT_EQ(42, *iot::validation::parsePositiveId("42"));
}

TEST_CASE(controller_id_parser_rejects_empty_zero_negative_and_partial_values) {
  EXPECT_FALSE(iot::validation::parsePositiveId("").has_value());
  EXPECT_FALSE(iot::validation::parsePositiveId("0").has_value());
  EXPECT_FALSE(iot::validation::parsePositiveId("-1").has_value());
  EXPECT_FALSE(iot::validation::parsePositiveId(" 42").has_value());
  EXPECT_FALSE(iot::validation::parsePositiveId("42 ").has_value());
  EXPECT_FALSE(iot::validation::parsePositiveId("42oops").has_value());
  EXPECT_FALSE(iot::validation::parsePositiveId("2147483648").has_value());
}

TEST_CASE(controller_chat_id_validation_allows_optional_and_negative_group_ids) {
  EXPECT_TRUE(iot::validation::isValidOptionalTelegramChatId(""));
  EXPECT_TRUE(iot::validation::isValidOptionalTelegramChatId("123456"));
  EXPECT_TRUE(iot::validation::isValidOptionalTelegramChatId("-100123456"));
}

TEST_CASE(controller_chat_id_validation_rejects_non_numeric_values) {
  EXPECT_FALSE(iot::validation::isValidOptionalTelegramChatId("-"));
  EXPECT_FALSE(iot::validation::isValidOptionalTelegramChatId("+123"));
  EXPECT_FALSE(iot::validation::isValidOptionalTelegramChatId("12a3"));
  EXPECT_FALSE(iot::validation::isValidOptionalTelegramChatId("12 3"));
}

TEST_CASE(controller_telemetry_validation_accepts_finite_temperatures) {
  EXPECT_TRUE(iot::validation::isValidTelemetryInput(1, 21.5));
  EXPECT_TRUE(iot::validation::isValidTelemetryInput(12, -18.0));
  EXPECT_TRUE(iot::validation::isValidTelemetryInput(12, 0.0));
}

TEST_CASE(controller_telemetry_validation_rejects_invalid_ids_and_non_finite_values) {
  EXPECT_FALSE(iot::validation::isValidTelemetryInput(0, 20.0));
  EXPECT_FALSE(iot::validation::isValidTelemetryInput(-1, 20.0));
  EXPECT_FALSE(iot::validation::isValidTelemetryInput(1, std::numeric_limits<double>::quiet_NaN()));
  EXPECT_FALSE(iot::validation::isValidTelemetryInput(1, std::numeric_limits<double>::infinity()));
  EXPECT_FALSE(iot::validation::isValidTelemetryInput(1, -std::numeric_limits<double>::infinity()));
}

TEST_CASE(controller_device_validation_requires_owner_and_non_empty_fields) {
  EXPECT_TRUE(iot::validation::isValidDeviceFields(1, "Boiler", "temperature"));
  EXPECT_FALSE(iot::validation::isValidDeviceFields(0, "Boiler", "temperature"));
  EXPECT_FALSE(iot::validation::isValidDeviceFields(1, "", "temperature"));
  EXPECT_FALSE(iot::validation::isValidDeviceFields(1, "Boiler", ""));
  EXPECT_FALSE(iot::validation::isValidDeviceFields(1, "   ", "temperature"));
  EXPECT_FALSE(iot::validation::isValidDeviceFields(1, "Boiler", "\t"));
}

TEST_CASE(controller_user_validation_requires_non_empty_email_password_and_name) {
  EXPECT_TRUE(iot::validation::isValidUserFields("user@example.test", "secret", "Alice"));
  EXPECT_FALSE(iot::validation::isValidUserFields("", "secret", "Alice"));
  EXPECT_FALSE(iot::validation::isValidUserFields("user@example.test", "", "Alice"));
  EXPECT_FALSE(iot::validation::isValidUserFields("user@example.test", "   ", "Alice"));
  EXPECT_FALSE(iot::validation::isValidUserFields("user@example.test", "secret", "  "));
}

TEST_CASE(controller_notification_validation_requires_chat_device_and_finite_numbers) {
  EXPECT_TRUE(iot::validation::isValidNotificationFields("chat-1", "Boiler", 80.0, 70.0));
  EXPECT_FALSE(iot::validation::isValidNotificationFields("", "Boiler", 80.0, 70.0));
  EXPECT_FALSE(iot::validation::isValidNotificationFields("chat-1", "", 80.0, 70.0));
  EXPECT_FALSE(iot::validation::isValidNotificationFields("chat-1", "   ", 80.0, 70.0));
  EXPECT_FALSE(iot::validation::isValidNotificationFields("chat-1", "Boiler", std::numeric_limits<double>::quiet_NaN(), 70.0));
  EXPECT_FALSE(iot::validation::isValidNotificationFields("chat-1", "Boiler", 80.0, std::numeric_limits<double>::infinity()));
}
