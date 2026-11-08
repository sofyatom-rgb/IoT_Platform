#pragma once

#include <drogon/drogon.h>

#include <functional>
#include <string>
#include <vector>

struct Rule {
  int rule_id;
  int device_id;
  std::string op;   // ">", "<", "=", "!="
  double value;
};

struct NotificationTargets {
  std::string chatId;
  std::string email;
  std::string deviceName;
};

// Репозиторий правил: работает с таблицей rules в PostgreSQL.
class RuleRepository {
 public:
  explicit RuleRepository(drogon::orm::DbClientPtr db);

  // Создать правило
  void createRule(
      int deviceId,
      const std::string& op,
      double value,
      std::function<void(int)> success,   // returns new rule_id
      std::function<void(const std::string&)> error) const;

  // Получить правило по id
  void getRuleById(
      int ruleId,
      std::function<void(const Rule&)> success,
      std::function<void(const std::string&)> error) const;

  // Получить все правила для устройства
  void getRulesForDevice(
      int deviceId,
      std::function<void(const std::vector<Rule>&)> success,
      std::function<void(const std::string&)> error) const;

  // Обновить правило
  void updateRule(
      int ruleId,
      int deviceId,
      const std::string& op,
      double value,
      std::function<void()> success,
      std::function<void(const std::string&)> error) const;

  // Удалить правило по id
  void deleteRule(
      int ruleId,
      std::function<void()> success,
      std::function<void(const std::string&)> error) const;

  // Получить контакты владельца устройства и имя устройства
  void getNotificationTargetsForDevice(
      int deviceId,
      std::function<void(const NotificationTargets&)> success,
      std::function<void(const std::string&)> error) const;

 private:
  drogon::orm::DbClientPtr db_;
};