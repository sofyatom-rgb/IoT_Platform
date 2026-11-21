#pragma once

#include <drogon/drogon.h>

#include <functional>
#include <string>
#include <vector>

/// Правило сравнения температуры с заданным порогом.
struct Rule {
  int rule_id;  ///< Идентификатор правила.
  std::string op;  ///< Оператор сравнения: >, <, = или !=.
  double value;    ///< Пороговое значение температуры.
};

/// Выполняет SQL-запросы к таблице rules.
class RuleRepository {
 public:
  /// Создаёт репозиторий с подключением к PostgreSQL.
  explicit RuleRepository(drogon::orm::DbClientPtr db);

  /// Создаёт правило для устройства и возвращает его ID.
  void createRule(int deviceId, const std::string &op, double value,
                  std::function<void(int)> success,
                  std::function<void(const std::string &)> error) const;

  /// Находит правило по ID.
  void getRuleById(int ruleId, std::function<void(const Rule &)> success,
                   std::function<void(const std::string &)> error) const;

  /// Возвращает все правила устройства.
  void getRulesForDevice(int deviceId,
                         std::function<void(const std::vector<Rule> &)> success,
                         std::function<void(const std::string &)> error) const;

  /// Изменяет оператор и порог правила.
  void updateRule(int ruleId, const std::string &op, double value,
                  std::function<void()> success,
                  std::function<void(const std::string &)> error) const;

  /// Удаляет правило по ID.
  void deleteRule(int ruleId, std::function<void()> success,
                  std::function<void(const std::string &)> error) const;

  /// Возвращает Telegram chat ID владельца устройства.
  void getChatIdForDevice(int deviceId,
                          std::function<void(const std::string &)> success,
                          std::function<void(const std::string &)> error) const;

  /// Привязывает правило к устройству.
  void attachToDevice(int ruleId, int deviceId,
                      std::function<void()> success,
                      std::function<void(const std::string &)> error) const;

  /// Удаляет привязку правила к устройству.
  void detachFromDevice(int ruleId, int deviceId,
                        std::function<void()> success,
                        std::function<void(const std::string &)> error) const;

 private:
  drogon::orm::DbClientPtr db_;
};
