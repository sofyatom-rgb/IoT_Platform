#include "services/NotificationService.h"

#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

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
      case '"':
        escaped += "&quot;";
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

std::string buildPlainText(const std::string &deviceName, double temperature,
                           double threshold) {
  return std::string("Превышен порог температуры\r\n\r\n") +
         "Устройство: " + deviceName + "\r\n" +
         "Температура: " + formatTemperature(temperature) + " °C\r\n" +
         "Порог: " + formatTemperature(threshold) + " °C\r\n";
}

// HTML для писем: таблицы, инлайн-стили, контейнер 600px (Outlook/Gmail).
std::string buildHtmlEmail(const std::string &deviceName, double temperature,
                           double threshold) {
  const std::string device = escapeHtml(deviceName);
  const std::string temp = formatTemperature(temperature);
  const std::string thr = formatTemperature(threshold);
  return "<!DOCTYPE html><html lang=\"ru\"><head><meta charset=\"UTF-8\">"
         "<title>Превышен порог температуры</title></head>"
         "<body style=\"margin:0;padding:0;background-color:#f4f6f8;\">"
         "<table role=\"presentation\" width=\"100%\" cellspacing=\"0\" "
         "cellpadding=\"0\" style=\"background-color:#f4f6f8;\"><tr><td "
         "align=\"center\"><table role=\"presentation\" width=\"600\" "
         "cellspacing=\"0\" cellpadding=\"0\" "
         "style=\"background-color:#ffffff;border-radius:8px;"
         "overflow:hidden;margin:24px 0;\">"
         "<tr><td style=\"background-color:#ff8c1a;padding:20px 32px;"
         "font-family:Arial,sans-serif;\">"
         "<div style=\"color:#ffffff;font-size:20px;font-weight:bold;\">"
         "IoT Platform — оповещение</div>"
         "<div style=\"color:#fff2e0;font-size:13px;margin-top:4px;\">"
         "Превышен порог температуры</div></td></tr>"
         "<tr><td style=\"padding:28px 32px;font-family:Arial,sans-serif;"
         "color:#222222;font-size:15px;line-height:1.6;\">"
         "<p style=\"margin:0 0 16px;\">Устройство <strong>" +
         device + "</strong> передает температуру выше заданного порога:</p>"
         "<table role=\"presentation\" width=\"100%\" cellspacing=\"0\" "
         "cellpadding=\"0\" style=\"font-size:15px;\">"
         "<tr><td style=\"padding:8px 12px;background-color:#fff7ed;"
         "border-radius:6px;font-weight:bold;color:#c2410c;\">"
         "Температура: " +
         temp + " °C</td>"
         "<td style=\"padding:8px 12px;color:#666666;\">Порог: " + thr +
         " °C</td></tr></table>"
         "<p style=\"margin:20px 0 0;color:#666666;font-size:13px;\">"
         "Проверьте состояние датчика и микроклимат.</p>"
         "</td></tr>"
         "<tr><td style=\"padding:0 32px 24px;font-family:Arial,sans-serif;"
         "font-size:12px;color:#999999;\">"
         "Письмо сформировано автоматически системой IoT Platform.</td></tr>"
         "</table></td></tr></table></body></html>";
}

std::string buildSubject(const std::string &deviceName, double temperature) {
  return "Превышен порог температуры: " + deviceName + " (" +
         formatTemperature(temperature) + " °C)";
}
}  // namespace

NotificationService::NotificationService(
    TelegramBotClient telegramBotClient,
    std::optional<EmailClient> emailClient,
    std::shared_ptr<TelegramThreadPool> threadPool)
    : telegramBotClient_(std::move(telegramBotClient)),
      emailClient_(std::move(emailClient)),
      threadPool_(std::move(threadPool)) {}

void NotificationService::sendTemperatureAlert(
    const std::string &chatId, const std::string &email,
    const std::string &deviceName, double temperature, double threshold,
    std::function<void(const ChannelStatuses &)> success,
    std::function<void(const std::string &)> error) const {
  ChannelStatuses statuses;

  const bool wantTelegram = !chatId.empty();
  const bool wantEmail = !email.empty();
  // Не настроенный канал = skipped, даже если адрес/чат_id передан.
  const bool telegramEnabled = telegramBotClient_.isEnabled();
  // EmailClient всегда есть как объект, но isEnabled() читает SmtpConfig.
  const bool emailEnabled = emailClient_.has_value() && emailClient_->isEnabled();

  const bool tryTelegram = wantTelegram && telegramEnabled;
  const bool tryEmail = wantEmail && emailEnabled;

  if (!wantTelegram || !telegramEnabled) {
    statuses.telegram = "skipped";
  }
  if (!wantEmail || !emailEnabled) {
    statuses.email = "skipped";
  }

  const int totalToTry = (tryTelegram ? 1 : 0) + (tryEmail ? 1 : 0);
  if (totalToTry == 0) {
    if (!wantTelegram && !wantEmail) {
      error("No notification channel provided");
    } else {
      error("No notification channel is configured");
    }
    return;
  }

  auto done = std::make_shared<int>(0);
  auto state = std::make_shared<ChannelStatuses>(statuses);

  auto finishOne = [done, state, totalToTry, tryTelegram, tryEmail,
                    success = std::move(success),
                    error = std::move(error)]() {
    ++(*done);
    if (*done < totalToTry) {
      return;
    }
    const std::string telegram = state->telegram;
    const std::string email = state->email;
    const bool telegramSent = (telegram == "sent");
    const bool emailSent = (email == "sent");
    if (telegramSent || emailSent) {
      success(*state);
      return;
    }
    std::string parts;
    if (tryTelegram) {
      parts += "telegram: " + telegram;
    }
    if (tryEmail) {
      if (!parts.empty()) {
        parts += "; ";
      }
      parts += "email: " + email;
    }
    error(parts);
  };

  if (tryTelegram) {
    const std::string message =
        "⚠️ <b>Превышен порог температуры</b>\n\n"
        "Устройство: <b>" +
        escapeHtml(deviceName) + "</b>\n" +
        "Температура: <b>" + formatTemperature(temperature) + " °C</b>\n" +
        "Порог: " + formatTemperature(threshold) + " °C";
    telegramBotClient_.sendMessage(
        chatId, message,
        [state, finishOne]() {
          state->telegram = "sent";
          finishOne();
        },
        [state, finishOne](const std::string &message) {
          state->telegram = "failed: " + message;
          finishOne();
        });
  }

  if (tryEmail) {
    const std::string subject = buildSubject(deviceName, temperature);
    const std::string plain = buildPlainText(deviceName, temperature, threshold);
    const std::string html = buildHtmlEmail(deviceName, temperature, threshold);
    emailClient_->sendMessage(
        email, subject, plain, html,
        [state, finishOne]() {
          state->email = "sent";
          finishOne();
        },
        [state, finishOne](const std::string &message) {
          state->email = "failed: " + message;
          finishOne();
        });
  }
}
