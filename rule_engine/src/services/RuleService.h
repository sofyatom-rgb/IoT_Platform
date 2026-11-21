#pragma once

#include <drogon/drogon.h>

#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "clients/NotificationClient.h"
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
      std::function<void(const std::string&)> success,
      std::function<void(const std::string&)> error) const;

  // Сбросить 15-минутный тайм-аут уведомлений для устройства.
  static void resetNotificationCooldown(int deviceId);

  // Проверить, активен ли тайм-аут уведомлений для устройства.
  static bool isNotificationOnCooldown(int deviceId);

  // Отметить, что уведомление для устройства было отправлено.
  static void markNotificationSent(int deviceId);

 private:
  static std::mutex &cooldownMutex();
  static std::unordered_map<int, std::chrono::steady_clock::time_point> &cooldownMap();
  drogon::orm::DbClientPtr db_;
  std::shared_ptr<NotificationClient> notificationClient_;
};