#pragma once

#include <drogon/drogon.h>

#include <functional>
#include <string>
#include <vector>

#include "repositories/RuleRepository.h"

/// Проверяет показания по правилам и отправляет уведомления о нарушениях.
class RuleService {
 public:
  /// Создаёт сервис правил с БД и адресом notification_service.
  RuleService(drogon::orm::DbClientPtr db, std::string notificationServiceUrl);

  /// Создаёт правило для устройства.
  void createRule(int deviceId, const std::string &op, double value,
                  std::function<void(int)> success,
                  std::function<void(const std::string &)> error) const;

  /// Возвращает правило по ID.
  void getRuleById(int ruleId, std::function<void(const Rule &)> success,
                   std::function<void(const std::string &)> error) const;

  /// Возвращает правила устройства.
  void getRulesForDevice(int deviceId,
                         std::function<void(const std::vector<Rule> &)> success,
                         std::function<void(const std::string &)> error) const;

  /// Изменяет оператор и порог правила.
  void updateRule(int ruleId, const std::string &op, double value,
                  std::function<void()> success,
                  std::function<void(const std::string &)> error) const;

  /// Удаляет правило.
  void deleteRule(int ruleId, std::function<void()> success,
                  std::function<void(const std::string &)> error) const;

  /// Проверяет температуру и отправляет уведомление при нарушении правила.
  void checkAndNotify(int deviceId, double temperature,
                      std::function<void()> success,
                      std::function<void(const std::string &)> error) const;

  /// Привязывает правило к устройству.
  void attachToDevice(int ruleId, int deviceId, std::function<void()> success,
                      std::function<void(const std::string &)> error) const;

  /// Снимает привязку правила к устройству.
  void detachFromDevice(int ruleId, int deviceId, std::function<void()> success,
                        std::function<void(const std::string &)> error) const;

 private:
  drogon::orm::DbClientPtr db_;
  std::string notificationUrl_;
};
