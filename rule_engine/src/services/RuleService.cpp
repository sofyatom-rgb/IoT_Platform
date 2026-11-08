#include "services/RuleService.h"

#include "clients/NotificationClient.h"
#include "repositories/RuleRepository.h"

#include <utility>

RuleService::RuleService(drogon::orm::DbClientPtr db,
                         std::string notificationServiceUrl)
    : db_(std::move(db)), notificationUrl_(std::move(notificationServiceUrl)) {}

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
    std::function<void()> success,
    std::function<void(const std::string&)> error) const {
  auto repo = std::make_shared<RuleRepository>(db_);
  std::string url = notificationUrl_;
  // 1. Получаем правила для устройства
  repo->getRulesForDevice(
      deviceId,
      [url, deviceId, temperature, repo, success,
       error](const std::vector<Rule>& rules) mutable {
        if (rules.empty()) {
          success();  // правил нет — ничего не проверяем
          return;
        }

        // 2. Получаем chat_id владельца устройства
        repo->getChatIdForDevice(
            deviceId,
            [url, deviceId, temperature, rules, repo, success,
             error](const std::string& chatId) mutable {
              if (chatId.empty()) {
                success();  // чат не настроен — уведомлять некому
                return;
              }

              // 3. Проверяем каждое правило
              bool violated = false;
              
              double triggeredThreshold = 0.0;
              for (const auto& rule : rules) {
                bool trigger = false;
                if (rule.op == ">") trigger = temperature > rule.value;
                if (rule.op == "<") trigger = temperature < rule.value;
                if (rule.op == "=") trigger = temperature == rule.value;
                if (rule.op == "!=") trigger = temperature != rule.value;

                if (trigger) {
                  violated = true;
                  triggeredThreshold = rule.value;
                  break;
                }
              }

              if (!violated) {
                success();  // все правила в норме
                return;
              }

              // 4. Отправляем уведомление через notification_service
              auto client = std::make_shared<NotificationClient>(url);
              client->sendTemperatureAlert(
                  chatId,
                  "device_" + std::to_string(deviceId),
                  temperature,
                  triggeredThreshold,
                  [success]() { success(); },
                  [error](const std::string& err) { error(err); });
            },
            error);
      },
      error);
}