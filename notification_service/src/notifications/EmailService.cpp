#include "notifications/EmailService.h"

#include <boost/asio/ssl/context.hpp>
#include <mailio/mailboxes.hpp>
#include <mailio/message.hpp>
#include <mailio/smtp.hpp>

#include <chrono>
#include <exception>
#include <random>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

namespace {
bool isValidEmail(const std::string &email) {
  const auto at = email.find('@');
  return at != std::string::npos && at != 0 && at + 1 < email.size() &&
         email.find('@', at + 1) == std::string::npos &&
         email.find_first_of(" \r\n\t") == std::string::npos;
}

std::string makeMultipartBoundary() {
  std::random_device source;
  std::ostringstream boundary;
  boundary << "iot-platform-" << std::hex << source() << source() << source()
           << source();
  return boundary.str();
}

std::string hidePassword(std::string error, const std::string &password) {
  if (password.empty()) {
    return error;
  }
  std::size_t position = 0;
  while ((position = error.find(password, position)) != std::string::npos) {
    error.replace(position, password.size(), "[redacted]");
    position += sizeof("[redacted]") - 1;
  }
  return error;
}

mailio::message makeMessage(const EmailConfig &config,
                            const std::string &recipient,
                            const std::string &recipientName,
                            const RenderedEmail &email) {
  mailio::message message;
  message.from(mailio::mail_address(config.senderName, config.fromEmail));
  message.add_recipient(mailio::mail_address(recipientName, recipient));
  message.subject(email.subject,
                  mailio::codec::codec_t::QUOTED_PRINTABLE);
  message.content_type(mailio::mime::media_type_t::MULTIPART, "alternative");
  message.content_type().boundary(makeMultipartBoundary());

  mailio::mime textPart;
  textPart.content_type(mailio::mime::media_type_t::TEXT, "plain", "UTF-8");
  textPart.content_transfer_encoding(
      mailio::mime::content_transfer_encoding_t::QUOTED_PRINTABLE);
  textPart.content(email.plainText);
  message.add_part(textPart);

  mailio::mime htmlPart;
  htmlPart.content_type(mailio::mime::media_type_t::TEXT, "html", "UTF-8");
  htmlPart.content_transfer_encoding(
      mailio::mime::content_transfer_encoding_t::QUOTED_PRINTABLE);
  htmlPart.content(email.html);
  message.add_part(htmlPart);
  return message;
}
}  // namespace

EmailService::EmailService(EmailConfig config) : config_(std::move(config)) {}

EmailSendResult EmailService::send(const std::string &recipient,
                                   const EmailAlertData &alert) const {
  const std::string configError = config_.validationError();
  if (!configError.empty()) {
    return {false, configError};
  }
  if (!isValidEmail(recipient)) {
    return {false, "recipient must be a valid email address"};
  }

  try {
    return sendRendered(recipient,
                        templates_.render(alert, config_.platformUrl),
                        alert.recipientName);
  } catch (const std::exception &exception) {
    return {false, hidePassword(exception.what(), config_.password)};
  }
}

EmailSendResult EmailService::sendTestEmail(
    const std::string &recipient) const {
  EmailAlertData alert;
  alert.platformName = "IoT Monitor";
  alert.eventType = "Тестовое уведомление";
  alert.sensorName = "Тестовый датчик";
  alert.sensorId = "test";
  alert.readingValue = "42";
  alert.unit = "°C";
  alert.threshold = "30";
  alert.location = "Тест";
  alert.severity = "INFO";
  alert.description =
      "Тестовое письмо подтверждает, что отправка уведомлений настроена.";
  return send(recipient, alert);
}

EmailSendResult EmailService::sendRendered(
    const std::string &recipient, const RenderedEmail &email,
    const std::string &recipientName) const {
  mailio::message message =
      makeMessage(config_, recipient, recipientName, email);
  const auto timeout =
      std::chrono::seconds(config_.timeoutSeconds);
  mailio::smtp connection(config_.host, config_.port,
                          std::chrono::duration_cast<std::chrono::milliseconds>(
                              timeout));
  const bool encrypted = config_.security != "none";
  connection.start_tls(config_.security == "starttls");
  if (encrypted) {
    const mailio::dialog_ssl::ssl_options_t sslOptions{
        boost::asio::ssl::context::tls_client, boost::asio::ssl::verify_peer};
    connection.ssl_options(sslOptions);
  } else {
    connection.ssl_options(std::nullopt);
  }
  connection.authenticate(
      config_.username, config_.password,
      encrypted ? mailio::smtp::auth_method_t::LOGIN
                : mailio::smtp::auth_method_t::NONE);
  connection.submit(message);
  return {true, {}};
}
