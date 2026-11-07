#pragma once

#include <string>

#include "notifications/EmailConfig.h"
#include "notifications/EmailTemplate.h"

struct EmailSendResult {
  bool sent = false;
  std::string error;
};

class EmailService {
 public:
  explicit EmailService(EmailConfig config);

  const EmailConfig &config() const { return config_; }
  bool isEnabled() const { return config_.enabled; }
  EmailSendResult send(const std::string &recipient,
                       const EmailAlertData &alert) const;
  EmailSendResult sendTestEmail(const std::string &recipient) const;

 private:
  EmailSendResult sendRendered(const std::string &recipient,
                               const RenderedEmail &email,
                               const std::string &recipientName) const;

  EmailConfig config_;
  EmailTemplate templates_;
};
