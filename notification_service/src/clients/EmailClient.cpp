#include "clients/EmailClient.h"

#include <curl/curl.h>
#include <drogon/drogon.h>

#include <algorithm>
#include <ctime>
#include <cstring>
#include <memory>
#include <string>
#include <utility>

namespace {
const char kBoundary[] = "IoTPlatformBoundary7f3a9e1";

struct UploadContext {
  std::string data;
  std::size_t position = 0;
};

std::size_t readCallback(void *dest, std::size_t size, std::size_t nmemb,
                         void *user) {
  auto *ctx = static_cast<UploadContext *>(user);
  const std::size_t unit = size * nmemb;
  const std::size_t remaining = ctx->data.size() - ctx->position;
  const std::size_t count = unit < remaining ? unit : remaining;
  if (count == 0) {
    return 0;
  }
  std::memcpy(dest, ctx->data.data() + ctx->position, count);
  ctx->position += count;
  return count;
}

std::string base64Encode(const std::string &input) {
  static const char kTable[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  out.reserve(((input.size() + 2) / 3) * 4);
  std::size_t i = 0;
  while (i < input.size()) {
    const std::size_t bytesLeft = input.size() - i;
    const unsigned char b0 =
        static_cast<unsigned char>(input[i]);
    const unsigned char b1 =
        bytesLeft > 1 ? static_cast<unsigned char>(input[i + 1]) : 0;
    const unsigned char b2 =
        bytesLeft > 2 ? static_cast<unsigned char>(input[i + 2]) : 0;
    out += kTable[b0 >> 2];
    out += kTable[((b0 & 0x03) << 4) | (b1 >> 4)];
    out += (bytesLeft > 1) ? kTable[((b1 & 0x0f) << 2) | (b2 >> 6)] : '=';
    out += (bytesLeft > 2) ? kTable[b2 & 0x3f] : '=';
    i += 3;
  }
  return out;
}

std::string wrapBase64(const std::string &encoded, std::size_t width = 76) {
  std::string out;
  for (std::size_t i = 0; i < encoded.size(); i += width) {
    out += encoded.substr(i, width);
    out += "\r\n";
  }
  return out;
}

// RFC 2047: кириллица в заголовках только через encoded-word.
std::string encodeSubject(const std::string &subject) {
  const bool needsEncoding =
      std::any_of(subject.begin(), subject.end(),
                  [](char c) { return static_cast<unsigned char>(c) > 127; });
  if (!needsEncoding) {
    return subject;
  }
  return "=?UTF-8?B?" + base64Encode(subject) + "?=";
}

std::string formatHttpDate() {
  const std::time_t now = std::time(nullptr);
  char buffer[64];
  std::strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S +0000",
                std::gmtime(&now));
  return buffer;
}

std::string extractEmailAddress(const std::string &from) {
  const auto open = from.find('<');
  const auto close = from.find('>', open);
  if (open != std::string::npos && close != std::string::npos &&
      close > open) {
    return from.substr(open + 1, close - open - 1);
  }
  return from;
}

class CurlHandleDeleter {
 public:
  void operator()(CURL *handle) const {
    if (handle != nullptr) {
      curl_easy_cleanup(handle);
    }
  }
};

using CurlHandlePtr = std::unique_ptr<CURL, CurlHandleDeleter>;

}  // namespace

void initCurlGlobal() { curl_global_init(CURL_GLOBAL_ALL); }

EmailClient::EmailClient(SmtpConfig config,
                         std::shared_ptr<TelegramThreadPool> threadPool)
    : config_(std::move(config)), threadPool_(std::move(threadPool)) {}

std::string EmailClient::buildMime(const SmtpConfig &config,
                                   const std::string &to,
                                   const std::string &subject,
                                   const std::string &plainText,
                                   const std::string &html) {
  std::string mime;
  mime += "From: " + config.from + "\r\n";
  mime += "To: " + to + "\r\n";
  mime += "Subject: " + encodeSubject(subject) + "\r\n";
  mime += "Date: " + formatHttpDate() + "\r\n";
  mime += "MIME-Version: 1.0\r\n";
  mime += std::string("Content-Type: multipart/alternative; boundary=\"") +
          kBoundary + "\"\r\n";
  mime += "\r\n";

  mime += "--";
  mime += kBoundary;
  mime += "\r\n";
  mime += "Content-Type: text/plain; charset=UTF-8\r\n";
  mime += "Content-Transfer-Encoding: base64\r\n";
  mime += "\r\n";
  mime += wrapBase64(base64Encode(plainText));

  mime += "--";
  mime += kBoundary;
  mime += "\r\n";
  mime += "Content-Type: text/html; charset=UTF-8\r\n";
  mime += "Content-Transfer-Encoding: base64\r\n";
  mime += "\r\n";
  mime += wrapBase64(base64Encode(html));

  mime += "--";
  mime += kBoundary;
  mime += "--\r\n";
  return mime;
}

void EmailClient::sendMessage(const std::string &to,
                              const std::string &subject,
                              const std::string &plainText,
                              const std::string &html,
                              std::function<void()> success,
                              std::function<void(const std::string &)> error)
    const {
  if (!isEnabled() || !threadPool_) {
    error("SMTP is not configured");
    return;
  }

  const std::string mime = buildMime(config_, to, subject, plainText, html);
  const std::string mailFrom = extractEmailAddress(config_.from);

  trantor::EventLoop *loop = trantor::EventLoop::getEventLoopOfCurrentThread();
  if (loop == nullptr) {
    loop = drogon::app().getLoop();
  }

  threadPool_->post([this, loop, to, mime, mailFrom,
                     success = std::move(success),
                     error = std::move(error)]() mutable {
    CurlHandlePtr curl(curl_easy_init());
    if (!curl) {
      const std::string message = "SMTP client init failed";
      loop->queueInLoop(
          [error = std::move(error), message]() { error(message); });
      return;
    }

    const std::string scheme =
        (config_.security == "tls") ? "smtps://" : "smtp://";
    const std::string url =
        scheme + config_.host + ":" + std::to_string(config_.port);

    char errorBuffer[CURL_ERROR_SIZE];
    errorBuffer[0] = '\0';

    UploadContext upload;
    upload.data = mime;

    curl_slist *recipients = nullptr;
    recipients = curl_slist_append(recipients, to.c_str());

    curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_READFUNCTION, &readCallback);
    curl_easy_setopt(curl.get(), CURLOPT_READDATA, &upload);
    curl_easy_setopt(curl.get(), CURLOPT_MAIL_FROM, mailFrom.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_MAIL_RCPT, recipients);
    curl_easy_setopt(curl.get(), CURLOPT_ERRORBUFFER, errorBuffer);
    curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT,
                     config_.timeoutSeconds);
    curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT,
                     config_.timeoutSeconds);

    if (config_.security == "tls" || config_.security == "starttls") {
      curl_easy_setopt(curl.get(), CURLOPT_USE_SSL, CURLUSESSL_ALL);
    } else {
      curl_easy_setopt(curl.get(), CURLOPT_USE_SSL, CURLUSESSL_NONE);
    }

    if (!config_.username.empty()) {
      curl_easy_setopt(curl.get(), CURLOPT_USERNAME,
                       config_.username.c_str());
    }
    if (!config_.password.empty()) {
      curl_easy_setopt(curl.get(), CURLOPT_PASSWORD,
                       config_.password.c_str());
      if (config_.security == "none") {
        // Разрешаем AUTH по обычному соединению (Mailpit не требует,
        // но Gmail-подобные серверы требуют STARTTLS — это настройка).
      }
    }

    const CURLcode result = curl_easy_perform(curl.get());
    const std::string detail = errorBuffer[0] != '\0'
                                   ? std::string(errorBuffer)
                                   : std::string(curl_easy_strerror(result));
    curl_slist_free_all(recipients);

    if (result != CURLE_OK) {
      const std::string message = "SMTP send failed: " + detail;
      loop->queueInLoop(
          [error = std::move(error), message]() { error(message); });
      return;
    }
    loop->queueInLoop([success = std::move(success)]() { success(); });
  });
}
