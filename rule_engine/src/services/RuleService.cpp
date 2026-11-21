#include "services/RuleService.h"

#include <drogon/drogon.h>

#include <utility>

#include "common/http/DrogonHttpTransport.h"
#include "repositories/RuleRepository.h"

RuleService::RuleService(drogon::orm::DbClientPtr db,
                         std::string notificationServiceUrl)
    : db_(std::move(db)),
      notificationClient_(std::make_shared<NotificationClient>(
          notificationServiceUrl,
          std::make_shared<iot::http::DrogonHttpTransport>(
              notificationServiceUrl))) {}

void RuleService::attachToDevice(
    int ruleId, int deviceId,
    std::function<void()> success,
    std::function<void(const std::string&)> error) const {
  auto repo = std::make_shared<RuleRepository>(db_);
  repo->attachToDevice(ruleId, deviceId, std::move(success), std::move(error));
}

void RuleService::detachFromDevice(
    int ruleId, int deviceId,
    std::function<void()> success,
    std::function<void(const std::string&)> error) const {
  auto repo = std::make_shared<RuleRepository>(db_);
  repo->detachFromDevice(ruleId, deviceId, std::move(success), std::move(error));
}

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
    const std::string& op,
    double value,
    std::function<void()> success,
    std::function<void(const std::string&)> error) const {
  auto repo = std::make_shared<RuleRepository>(db_);
  repo->updateRule(ruleId, op, value, std::move(success), std::move(error));
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
  LOG_INFO << "[rule_engine][service] checkAndNotify: started for device "
           << deviceId << ", temperature=" << temperature;

  // Ошибки уведомлений и БД пишем в журнал, но сохраняем исходный контракт ответа.
  const auto logError = [error](const std::string &message) {
    LOG_ERROR << "[rule_engine][service] checkAndNotify failed: " << message;
    error(message);
  };

  auto repo = std::make_shared<RuleRepository>(db_);
  auto notificationClient = notificationClient_;
  repo->getRulesForDevice(
      deviceId,
      [deviceId, temperature, repo, notificationClient, success,
       logError](const std::vector<Rule>& rules) mutable {
        if (rules.empty()) {
          LOG_INFO << "[rule_engine][service] checkAndNotify: no rules for device "
                   << deviceId;
          notificationClient->clearTemperatureAlert(
              deviceId,
              [success]() { success("no_rules"); }, logError);
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
          LOG_INFO << "[rule_engine][service] checkAndNotify: temperature is within "
                      "rules for device "
                   << deviceId;
          notificationClient->clearTemperatureAlert(
              deviceId,
              [success]() { success("not_triggered"); }, logError);
          return;
        }

        if (isNotificationOnCooldown(deviceId)) {
          LOG_INFO << "[rule_engine][service] checkAndNotify: notification is on "
                      "cooldown for device "
                   << deviceId;
          success("cooldown");
          return;
        }

        const double threshold = triggeredRule->value;
        LOG_WARN << "[rule_engine][service] checkAndNotify: rule violated for device "
                 << deviceId << ", threshold=" << threshold;
        repo->getNotificationTargetsForDevice(
            deviceId,
            [deviceId, temperature, threshold, notificationClient, success,
             logError](const NotificationTargets& targets) mutable {
              if (targets.chatId.empty() && targets.email.empty()) {
                LOG_WARN << "[rule_engine][service] checkAndNotify: no notification "
                            "recipients for device "
                         << deviceId;
                notificationClient->clearTemperatureAlert(
                    deviceId,
                    [success]() { success("no_recipients"); }, logError);
                return;
              }
              notificationClient->sendTemperatureAlert(
                  deviceId, targets.chatId, targets.email, targets.deviceName,
                  temperature, threshold,
                  [deviceId, success](const std::string &status) {
                    markNotificationSent(deviceId);
                    LOG_INFO << "[rule_engine][service] checkAndNotify: alert sent for "
                                "device "
                             << deviceId << ", status=" << status;
                    success(status);
                  },
                  logError);
            },
            logError);
      },
      logError);
}

std::mutex &RuleService::cooldownMutex() {
  static std::mutex mutex;
  return mutex;
}

std::unordered_map<int, std::chrono::steady_clock::time_point> &
RuleService::cooldownMap() {
  static std::unordered_map<int, std::chrono::steady_clock::time_point> map;
  return map;
}

void RuleService::resetNotificationCooldown(int deviceId) {
  std::lock_guard<std::mutex> lock(cooldownMutex());
  cooldownMap().erase(deviceId);
  LOG_INFO << "[rule_engine][service] notification cooldown reset for device "
           << deviceId;
}

bool RuleService::isNotificationOnCooldown(int deviceId) {
  std::lock_guard<std::mutex> lock(cooldownMutex());
  const auto it = cooldownMap().find(deviceId);
  if (it == cooldownMap().end()) {
    return false;
  }
  const auto elapsed =
      std::chrono::steady_clock::now() - it->second;
  if (elapsed >= std::chrono::minutes(15)) {
    cooldownMap().erase(it);
    return false;
  }
  return true;
}

void RuleService::markNotificationSent(int deviceId) {
  std::lock_guard<std::mutex> lock(cooldownMutex());
  cooldownMap()[deviceId] = std::chrono::steady_clock::now();
}
