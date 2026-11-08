#include "services/RuleService.h"

#include "common/http/DrogonHttpTransport.h"
#include "repositories/RuleRepository.h"

#include <utility>

RuleService::RuleService(drogon::orm::DbClientPtr db,
                         std::string notificationServiceUrl)
    : db_(std::move(db)),
      notificationClient_(std::make_shared<NotificationClient>(
          notificationServiceUrl,
          std::make_shared<iot::http::DrogonHttpTransport>(
              notificationServiceUrl))) {}

void RuleService::createRule(
    int deviceId,
    const std::string& op,
    double value,
    std::function<void(int)> success,
    std::function<void(const std::string&)> error) const {
  auto repo = std::make_shared<RuleRepository>(db_);
  repo->createRule(deviceId, op, value, std::move(success), std::move(error));
}

void RuleService::getRuleById(
    int ruleId,
    std::function<void(const Rule&)> success,
    std::function<void(const std::string&)> error) const {
  auto repo = std::make_shared<RuleRepository>(db_);
  repo->getRuleById(ruleId, std::move(success), std::move(error));
}

void RuleService::getRulesForDevice(
    int deviceId,
    std::function<void(const std::vector<Rule>&)> success,
    std::function<void(const std::string&)> error) const {
  auto repo = std::make_shared<RuleRepository>(db_);
  repo->getRulesForDevice(deviceId, std::move(success), std::move(error));
}

void RuleService::updateRule(
    int ruleId,
    int deviceId,
    const std::string& op,
    double value,
    std::function<void()> success,
    std::function<void(const std::string&)> error) const {
  auto repo = std::make_shared<RuleRepository>(db_);
  repo->updateRule(ruleId, deviceId, op, value, std::move(success), std::move(error));
}

void RuleService::deleteRule(
    int ruleId,
    std::function<void()> success,
    std::function<void(const std::string&)> error) const {
  auto repo = std::make_shared<RuleRepository>(db_);
  repo->deleteRule(ruleId, std::move(success), std::move(error));
}

void RuleService::checkAndNotify(
    int deviceId,
    double temperature,
    std::function<void(const std::string&)> success,
    std::function<void(const std::string&)> error) const {
  auto repo = std::make_shared<RuleRepository>(db_);
  auto notificationClient = notificationClient_;
  repo->getRulesForDevice(
      deviceId,
      [deviceId, temperature, repo, notificationClient, success,
       error](const std::vector<Rule>& rules) mutable {
        if (rules.empty()) {
          notificationClient->clearTemperatureAlert(
              deviceId,
              [success]() { success("no_rules"); }, error);
          return;
        }

        const Rule* triggeredRule = nullptr;
        for (const auto& rule : rules) {
          const bool trigger =
              (rule.op == ">" && temperature > rule.value) ||
              (rule.op == "<" && temperature < rule.value) ||
              (rule.op == "=" && temperature == rule.value) ||
              (rule.op == "!=" && temperature != rule.value);
          if (trigger) {
            triggeredRule = &rule;
            break;
          }
        }

        if (triggeredRule == nullptr) {
          notificationClient->clearTemperatureAlert(
              deviceId,
              [success]() { success("not_triggered"); }, error);
          return;
        }

        const double threshold = triggeredRule->value;
        repo->getNotificationTargetsForDevice(
            deviceId,
            [deviceId, temperature, threshold, notificationClient, success,
             error](const NotificationTargets& targets) mutable {
              if (targets.chatId.empty() && targets.email.empty()) {
                notificationClient->clearTemperatureAlert(
                    deviceId,
                    [success]() { success("no_recipients"); }, error);
                return;
              }
              notificationClient->sendTemperatureAlert(
                  deviceId, targets.chatId, targets.email, targets.deviceName,
                  temperature, threshold, success, error);
            },
            error);
      },
      error);
}