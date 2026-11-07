#pragma once

#include <charconv>
#include <cmath>
#include <optional>
#include <string_view>
#include <system_error>

// Pure validation helpers shared by HTTP controllers and their unit tests.
// This header intentionally has no Drogon or JsonCpp dependency.
namespace iot::validation {

inline bool isPositiveId(int value) noexcept { return value > 0; }

inline std::optional<int> parsePositiveId(std::string_view text) noexcept {
  if (text.empty()) return std::nullopt;

  int value = 0;
  const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
  if (result.ec != std::errc{} || result.ptr != text.data() + text.size() ||
      value <= 0) {
    return std::nullopt;
  }
  return value;
}

inline bool isFiniteNumber(double value) noexcept {
  return std::isfinite(value);
}

inline bool isValidTelemetryInput(int deviceId, double temperature) noexcept {
  return isPositiveId(deviceId) && isFiniteNumber(temperature);
}

// An empty chat id is allowed because Telegram notifications are optional.
// A non-empty id must consist only of decimal digits, optionally prefixed by '-'
// (Telegram group/channel chat ids can be negative).
inline bool isValidOptionalTelegramChatId(std::string_view chatId) noexcept {
  if (chatId.empty()) return true;

  std::size_t firstDigit = chatId.front() == '-' ? 1U : 0U;
  if (firstDigit == chatId.size()) return false;

  for (std::size_t i = firstDigit; i < chatId.size(); ++i) {
    const char ch = chatId[i];
    if (ch < '0' || ch > '9') return false;
  }
  return true;
}

inline bool hasNonWhitespace(std::string_view value) noexcept {
  for (const char ch : value) {
    if (ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r' && ch != '\f' && ch != '\v') {
      return true;
    }
  }
  return false;
}

inline bool isValidUserFields(std::string_view email, std::string_view password,
                              std::string_view name) noexcept {
  // Email syntax/uniqueness belongs to the domain/database; the HTTP layer
  // only rejects missing or whitespace-only required fields.
  return hasNonWhitespace(email) && hasNonWhitespace(password) &&
         hasNonWhitespace(name);
}

inline bool isValidDeviceFields(int ownerId, std::string_view name,
                                std::string_view type) noexcept {
  return isPositiveId(ownerId) && hasNonWhitespace(name) && hasNonWhitespace(type);
}

inline bool isValidNotificationFields(std::string_view chatId,
                                     std::string_view deviceName,
                                     double temperature,
                                     double threshold) noexcept {
  return !chatId.empty() && hasNonWhitespace(deviceName) &&
         isFiniteNumber(temperature) && isFiniteNumber(threshold);
}

}  // namespace iot::validation
