#include "notifications/EmailService.h"

#include <curl/curl.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <ctime>
#include <exception>
#include <iomanip>
#include <locale>
#include <memory>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {
bool isValidEmail(const std::string &email) {
  const auto at = email.find('@');
  return at != std::string::npos && at != 0 && at + 1 < email.size() &&
         email.find('@', at + 1) == std::string::npos &&
         email.find_first_of(" \r\n\t<>,;:\"[]") == std::string::npos;
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

std::string base64(std::string_view input) {
  constexpr std::string_view alphabet =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string output;
  output.reserve(((input.size() + 2) / 3) * 4);
  for (std::size_t index = 0; index < input.size(); index += 3) {
    const auto first = static_cast<unsigned char>(input[index]);
    const auto second = index + 1 < input.size()
                            ? static_cast<unsigned char>(input[index + 1])
                            : 0;
    const auto third = index + 2 < input.size()
                           ? static_cast<unsigned char>(input[index + 2])
                           : 0;
    output += alphabet[first >> 2];
    output += alphabet[((first & 0x03) << 4) | (second >> 4)];
    output += index + 1 < input.size()
                  ? alphabet[((second & 0x0f) << 2) | (third >> 6)]
                  : '=';
    output += index + 2 < input.size() ? alphabet[third & 0x3f] : '=';
  }
  return output;
}

std::string encodeHeaderWord(std::string_view value) {
  if (value.empty()) {
    return {};
  }

  std::string encoded;
  std::size_t offset = 0;
  while (offset < value.size()) {
    std::size_t end = std::min(offset + 45, value.size());
    while (end < value.size() &&
           (static_cast<unsigned char>(value[end]) & 0xc0) == 0x80) {
      --end;
    }
    if (end == offset) {
      end = std::min(offset + 45, value.size());
    }
    if (!encoded.empty()) {
      encoded += "\r\n ";
    }
    encoded += "=?UTF-8?B?";
    encoded += base64(value.substr(offset, end - offset));
    encoded += "?=";
    offset = end;
  }
  return encoded;
}

std::string formatMailbox(const std::string &displayName,
                          const std::string &email) {
  const std::string encodedName = encodeHeaderWord(displayName);
  if (encodedName.empty()) {
    return "<" + email + ">";
  }
  return encodedName + " <" + email + ">";
}

std::string quotedPrintable(std::string_view input) {
  std::string output;
  output.reserve(input.size() * 2);
  std::size_t lineLength = 0;
  for (std::size_t index = 0; index < input.size(); ++index) {
    const auto character = static_cast<unsigned char>(input[index]);
    if (character == '\r' || character == '\n') {
      if (character == '\r' && index + 1 < input.size() &&
          input[index + 1] == '\n') {
        ++index;
      }
      output += "\r\n";
      lineLength = 0;
      continue;
    }

    std::string token;
    if (character >= 33 && character <= 60) {
      token.push_back(static_cast<char>(character));
    } else if (character >= 62 && character <= 126) {
      token.push_back(static_cast<char>(character));
    } else {
      constexpr char hex[] = "0123456789ABCDEF";
      token = {'=', hex[character >> 4], hex[character & 0x0f]};
    }

    if (lineLength + token.size() > 75) {
      output += "=\r\n";
      lineLength = 0;
    }
    output += token;
    lineLength += token.size();
  }
  return output;
}

std::string makeDateHeader() {
  const std::time_t now =
      std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  std::tm utc{};
  if (gmtime_r(&now, &utc) == nullptr) {
    throw std::runtime_error("Could not format email date");
  }
  std::ostringstream date;
  date.imbue(std::locale::classic());
  date << std::put_time(&utc, "%a, %d %b %Y %H:%M:%S +0000");
  return date.str();
}

std::string makeMessage(const EmailConfig &config, const std::string &recipient,
                        const std::string &recipientName,
                        const RenderedEmail &email) {
  const std::string boundary = makeMultipartBoundary();
  std::ostringstream message;
  message << "Date: " << makeDateHeader() << "\r\n"
          << "From: " << formatMailbox(config.senderName, config.fromEmail)
          << "\r\n"
          << "To: " << formatMailbox(recipientName, recipient) << "\r\n"
          << "Subject: " << encodeHeaderWord(email.subject) << "\r\n"
          << "MIME-Version: 1.0\r\n"
          << "Content-Type: multipart/alternative; boundary=\"" << boundary
          << "\"\r\n"
          << "\r\n"
          << "--" << boundary << "\r\n"
          << "Content-Type: text/plain; charset=UTF-8\r\n"
          << "Content-Transfer-Encoding: quoted-printable\r\n"
          << "\r\n"
          << quotedPrintable(email.plainText) << "\r\n"
          << "--" << boundary << "\r\n"
          << "Content-Type: text/html; charset=UTF-8\r\n"
          << "Content-Transfer-Encoding: quoted-printable\r\n"
          << "\r\n"
          << quotedPrintable(email.html) << "\r\n"
          << "--" << boundary << "--\r\n";
  return message.str();
}

class CurlGlobal {
public:
  CurlGlobal() {
    const CURLcode result = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (result != CURLE_OK) {
      throw std::runtime_error(std::string("Could not initialize libcurl: ") +
                               curl_easy_strerror(result));
    }
  }

  ~CurlGlobal() { curl_global_cleanup(); }
};

void setOption(CURL *curl, CURLoption option, auto value) {
  const CURLcode result = curl_easy_setopt(curl, option, value);
  if (result != CURLE_OK) {
    throw std::runtime_error(std::string("Could not configure libcurl: ") +
                             curl_easy_strerror(result));
  }
}

struct UploadContext {
  const std::string *message;
  std::size_t offset = 0;
};

std::size_t readMessage(char *buffer, std::size_t size, std::size_t count,
                        void *userData) {
  auto &context = *static_cast<UploadContext *>(userData);
  if (size != 0 && count > static_cast<std::size_t>(-1) / size) {
    return CURL_READFUNC_ABORT;
  }
  const std::size_t capacity = size * count;
  const std::size_t remaining = context.message->size() - context.offset;
  const std::size_t copied = std::min(capacity, remaining);
  if (copied != 0) {
    std::copy_n(context.message->data() + context.offset, copied, buffer);
    context.offset += copied;
  }
  return copied;
}

std::size_t discardResponse(char *, std::size_t size, std::size_t count,
                            void *) {
  if (size != 0 && count > static_cast<std::size_t>(-1) / size) {
    return 0;
  }
  return size * count;
}

std::string makeSmtpUrl(const EmailConfig &config) {
  std::string host = config.host;
  if (host.find(':') != std::string::npos && host.front() != '[') {
    host = "[" + host + "]";
  }
  return std::string(config.security == "tls" ? "smtps://" : "smtp://") + host +
         ":" + std::to_string(config.port);
}
} // namespace

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

EmailSendResult
EmailService::sendTestEmail(const std::string &recipient) const {
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

EmailSendResult
EmailService::sendRendered(const std::string &recipient,
                           const RenderedEmail &email,
                           const std::string &recipientName) const {
  try {
    static const CurlGlobal curlGlobal;
    (void)curlGlobal;

    using CurlHandle = std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>;
    CurlHandle curl(curl_easy_init(), &curl_easy_cleanup);
    if (!curl) {
      throw std::runtime_error("Could not create libcurl SMTP handle");
    }

    curl_slist *recipients =
        curl_slist_append(nullptr, ("<" + recipient + ">").c_str());
    if (recipients == nullptr) {
      throw std::runtime_error("Could not allocate SMTP recipient list");
    }
    const auto cleanupRecipients = [](curl_slist *list) {
      curl_slist_free_all(list);
    };
    std::unique_ptr<curl_slist, decltype(cleanupRecipients)> recipientList(
        recipients, cleanupRecipients);

    const std::string message =
        makeMessage(config_, recipient, recipientName, email);
    UploadContext upload{&message};
    std::array<char, CURL_ERROR_SIZE> errorBuffer{};
    const std::string url = makeSmtpUrl(config_);
    const std::string sender = "<" + config_.fromEmail + ">";

    setOption(curl.get(), CURLOPT_URL, url.c_str());
    setOption(curl.get(), CURLOPT_MAIL_FROM, sender.c_str());
    setOption(curl.get(), CURLOPT_MAIL_RCPT, recipientList.get());
    setOption(curl.get(), CURLOPT_UPLOAD, 1L);
    setOption(curl.get(), CURLOPT_READFUNCTION, &readMessage);
    setOption(curl.get(), CURLOPT_READDATA, &upload);
    setOption(curl.get(), CURLOPT_INFILESIZE_LARGE,
              static_cast<curl_off_t>(message.size()));
    setOption(curl.get(), CURLOPT_WRITEFUNCTION, &discardResponse);
    setOption(curl.get(), CURLOPT_ERRORBUFFER, errorBuffer.data());
    setOption(curl.get(), CURLOPT_TIMEOUT,
              static_cast<long>(config_.timeoutSeconds));
    setOption(curl.get(), CURLOPT_CONNECTTIMEOUT,
              static_cast<long>(config_.timeoutSeconds));
    setOption(curl.get(), CURLOPT_NOSIGNAL, 1L);
    setOption(curl.get(), CURLOPT_SSL_VERIFYPEER, 1L);
    setOption(curl.get(), CURLOPT_SSL_VERIFYHOST, 2L);

    if (config_.security == "starttls") {
      setOption(curl.get(), CURLOPT_USE_SSL, static_cast<long>(CURLUSESSL_ALL));
    } else if (config_.security == "none") {
      setOption(curl.get(), CURLOPT_USE_SSL,
                static_cast<long>(CURLUSESSL_NONE));
    }
    if (config_.security != "none") {
      setOption(curl.get(), CURLOPT_USERNAME, config_.username.c_str());
      setOption(curl.get(), CURLOPT_PASSWORD, config_.password.c_str());
      setOption(curl.get(), CURLOPT_LOGIN_OPTIONS, "AUTH=LOGIN");
    }

    const CURLcode result = curl_easy_perform(curl.get());
    if (result != CURLE_OK) {
      const std::string detail = errorBuffer.front() == '\0'
                                     ? curl_easy_strerror(result)
                                     : errorBuffer.data();
      throw std::runtime_error("SMTP send failed: " + detail);
    }
    return {true, {}};
  } catch (const std::exception &exception) {
    return {false, hidePassword(exception.what(), config_.password)};
  }
}
