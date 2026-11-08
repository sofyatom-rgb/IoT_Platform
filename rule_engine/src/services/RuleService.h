#pragma once

#include <drogon/drogon.h>

#include <functional>
#include <string>
#include <vector>

#include "repositories/RuleRepository.h"

// Сервис правил: проверяет телеметрию против правил и при нарушении шлёт уведомление.
class RuleService {
 public:
  RuleService(drogon::orm::DbClientPtr db,
              std::string notificationServiceUrl);

  // CRUD правила
  void createRule(
      int deviceId,
      const std::string& op,
      double value,
      std::function<void(int)> success,
      std::function<void(const std::string&)> error) const;

  void getRuleById(
      int ruleId,
      std::function<void(const Rule&)> success,
      std::function<void(const std::string&)> error) const;

  void getRulesForDevice(
      int deviceId,
      std::function<void(const std::vector<Rule>&)> success,
      std::function<void(const std::string&)> error) const;

  void updateRule(
      int ruleId,
      int deviceId,
      const std::string& op,
      double value,
      std::function<void()> success,
      std::function<void(const std::string&)> error) const;

  void deleteRule(
      int ruleId,
      std::function<void()> success,
      std::function<void(const std::string&)> error) const;

  // Проверить телеметрию устройства против его правил.
  // При нарушении отправить уведомление в notification_service.
  void checkAndNotify(
      int deviceId,
      double temperature,
      std::function<void()> success,
      std::function<void(const std::string&)> error) const;

 private:
  drogon::orm::DbClientPtr db_;
  std::string notificationUrl_;
};