#include "repositories/RuleRepository.h"

#include <utility>

RuleRepository::RuleRepository(drogon::orm::DbClientPtr db)
    : db_(std::move(db)) {}

void RuleRepository::createRule(
    int deviceId,
    const std::string& op,
    double value,
    std::function<void(int)> success,
    std::function<void(const std::string&)> error) const {
  db_->execSqlAsync(
      "INSERT INTO rules (device_id, operator, value) VALUES ($1, $2, $3) "
      "RETURNING rule_id",
      [success](const drogon::orm::Result& r) {
        if (!r.empty()) {
          success(r[0]["rule_id"].as<int>());
        } else {
          success(-1);
        }
      },
      [error](const drogon::orm::DrogonDbException& e) {
        error(e.base().what());
      },
      deviceId, op, value);
}

void RuleRepository::getRuleById(
    int ruleId,
    std::function<void(const Rule&)> success,
    std::function<void(const std::string&)> error) const {
  db_->execSqlAsync(
      "SELECT rule_id, device_id, operator, value FROM rules WHERE rule_id = $1",
      [success, error](const drogon::orm::Result& r) {
        if (r.empty()) {
          error("Rule not found");
          return;
        }
        Rule rule;
        rule.rule_id = r[0]["rule_id"].as<int>();
        rule.device_id = r[0]["device_id"].as<int>();
        rule.op = r[0]["operator"].as<std::string>();
        rule.value = r[0]["value"].as<double>();
        success(rule);
      },
      [error](const drogon::orm::DrogonDbException& e) {
        error(e.base().what());
      },
      ruleId);
}

void RuleRepository::getRulesForDevice(
    int deviceId,
    std::function<void(const std::vector<Rule>&)> success,
    std::function<void(const std::string&)> error) const {
  db_->execSqlAsync(
      "SELECT rule_id, device_id, operator, value FROM rules WHERE device_id = $1",
      [success](const drogon::orm::Result& r) {
        std::vector<Rule> rules;
        rules.reserve(r.size());
        for (const auto& row : r) {
          Rule rule;
          rule.rule_id = row["rule_id"].as<int>();
          rule.device_id = row["device_id"].as<int>();
          rule.op = row["operator"].as<std::string>();
          rule.value = row["value"].as<double>();
          rules.push_back(std::move(rule));
        }
        success(rules);
      },
      [error](const drogon::orm::DrogonDbException& e) {
        error(e.base().what());
      },
      deviceId);
}

void RuleRepository::updateRule(
    int ruleId,
    int deviceId,
    const std::string& op,
    double value,
    std::function<void()> success,
    std::function<void(const std::string&)> error) const {
  db_->execSqlAsync(
      "UPDATE rules SET device_id = $1, operator = $2, value = $3 "
      "WHERE rule_id = $4",
      [success](const drogon::orm::Result&) {
        success();
      },
      [error](const drogon::orm::DrogonDbException& e) {
        error(e.base().what());
      },
      deviceId, op, value, ruleId);
}

void RuleRepository::deleteRule(
    int ruleId,
    std::function<void()> success,
    std::function<void(const std::string&)> error) const {
  db_->execSqlAsync(
      "DELETE FROM rules WHERE rule_id = $1",
      [success](const drogon::orm::Result&) {
        success();
      },
      [error](const drogon::orm::DrogonDbException& e) {
        error(e.base().what());
      },
      ruleId);
}

void RuleRepository::getNotificationTargetsForDevice(
    int deviceId,
    std::function<void(const NotificationTargets&)> success,
    std::function<void(const std::string&)> error) const {
  db_->execSqlAsync(
      "SELECT d.name AS device_name, u.telegram_chat_id, u.email "
      "FROM devices d "
      "JOIN users u ON u.id = d.owner_id "
      "WHERE d.id = $1",
      [success](const drogon::orm::Result& r) {
        NotificationTargets targets;
        if (!r.empty()) {
          targets.deviceName = r[0]["device_name"].as<std::string>();
          const auto chatId = r[0]["telegram_chat_id"];
          if (!chatId.isNull()) {
            targets.chatId = chatId.as<std::string>();
          }
          const auto email = r[0]["email"];
          if (!email.isNull()) {
            targets.email = email.as<std::string>();
          }
        }
        success(targets);
      },
      [error](const drogon::orm::DrogonDbException& e) {
        error(e.base().what());
      },
      deviceId);
}