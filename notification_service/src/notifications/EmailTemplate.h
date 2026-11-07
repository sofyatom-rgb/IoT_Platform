#pragma once

#include <string>

struct EmailAlertData {
  std::string recipientName;
  std::string platformName = "IoT Monitor";
  std::string eventType;
  std::string sensorName;
  std::string sensorId;
  std::string readingValue;
  std::string unit;
  std::string threshold;
  std::string location;
  std::string occurredAt;
  std::string severity = "INFO";
  std::string description;
};

struct RenderedEmail {
  std::string subject;
  std::string plainText;
  std::string html;
};

class EmailTemplate {
 public:
  explicit EmailTemplate(std::string templateDirectory = {});

  RenderedEmail render(const EmailAlertData &data,
                       const std::string &platformUrl) const;
  std::string renderHtml(const EmailAlertData &data,
                         const std::string &platformUrl) const;
  std::string renderPlainText(const EmailAlertData &data,
                              const std::string &platformUrl = {}) const;

  static std::string escapeHtml(const std::string &value);

 private:
  std::string htmlTemplate_;
  std::string textTemplate_;
};
