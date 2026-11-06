#include "services/NotificationService.h"

#include <iomanip>
#include <sstream>
#include <string>
#include <utility>

namespace {
std::string escapeHtml(const std::string &value) {
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
      default:
        escaped += character;
        break;
    }
  }
  return escaped;
}

std::string formatTemperature(double value) {
  std::ostringstream stream;
  stream << std::fixed << std::setprecision(2) << value;
  std::string formatted = stream.str();
  const auto dot = formatted.find('.');
  if (dot != std::string::npos) {
    const auto last = formatted.find_last_not_of('0');
    if (last == dot) {
      formatted.erase(dot);
    } else {
      formatted.erase(last + 1);
    }
  }
  return formatted;
}
}  // namespace

NotificationService::NotificationService(TelegramBotClient telegramBotClient)
    : telegramBotClient_(std::move(telegramBotClient)) {}

void NotificationService::sendTemperatureAlert(
    const std::string &chatId, const std::string &deviceName,
    double temperature, double threshold, std::function<void()> success,
    std::function<void(const std::string &)> error) const {
  const std::string message =
      "⚠️ <b>Превышен порог температуры</b>\n\n"
      "Устройство: <b>" +
      escapeHtml(deviceName) + "</b>\n" + "Температура: <b>" +
      formatTemperature(temperature) + " °C</b>\n" + "Порог: " +
      formatTemperature(threshold) + " °C";
  telegramBotClient_.sendMessage(chatId, message, std::move(success),
                                 std::move(error));
}
