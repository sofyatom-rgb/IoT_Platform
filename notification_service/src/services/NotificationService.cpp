#include "services/NotificationService.h"

#include <stdexcept>
#include <utility>

NotificationService::NotificationService(
    std::shared_ptr<ITelegramBotClient> telegramBotClient)
    : telegramBotClient_(std::move(telegramBotClient)) {
  if (!telegramBotClient_) {
    throw std::invalid_argument("NotificationService requires a Telegram client");
  }
}

void NotificationService::sendTemperatureAlert(
    const std::string &chatId, const std::string &deviceName,
    double temperature, double threshold, std::function<void()> success,
    std::function<void(const std::string &)> error) const {
  const std::string message =
      "Внимание! На устройстве \"" + deviceName + "\" температура " +
      std::to_string(temperature) + " °C превысила порог " +
      std::to_string(threshold) + " °C.";
  telegramBotClient_->sendMessage(chatId, message, std::move(success),
                                  std::move(error));
}
