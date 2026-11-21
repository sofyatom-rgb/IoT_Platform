#include "repositories/RuleRepository.h"

#include <utility>

RuleRepository::RuleRepository(drogon::orm::DbClientPtr db)
    : db_(std::move(db)) {}

void RuleRepository::attachToDevice(
    int ruleId, int deviceId, std::function<void()> success,
    std::function<void(const std::string &)> error) const {
  LOG_INFO << "[rule_engine][db] rules.attachToDevice: started";
  db_->execSqlAsync(
      "INSERT INTO device_to_rule (device_id, rule_id) VALUES ($1, $2) "
      "ON CONFLICT DO NOTHING",
      [success](const drogon::orm::Result &) {
        LOG_INFO << "[rule_engine][db] rules.attachToDevice: completed";
        success();
      },
      [error](const drogon::orm::DrogonDbException &e) {
        LOG_ERROR << "[rule_engine][db] rules.attachToDevice failed";
        error(e.base().what());
      },
      deviceId, ruleId);
}

void RuleRepository::detachFromDevice(
    int ruleId, int deviceId, std::function<void()> success,
    std::function<void(const std::string &)> error) const {
  LOG_INFO << "[rule_engine][db] rules.detachFromDevice: started";
  db_->execSqlAsync(
      "DELETE FROM device_to_rule WHERE device_id = $1 AND rule_id = $2",
      [success](const drogon::orm::Result &) {
        LOG_INFO << "[rule_engine][db] rules.detachFromDevice: completed";
        success();
      },
      [error](const drogon::orm::DrogonDbException &e) {
        LOG_ERROR << "[rule_engine][db] rules.detachFromDevice failed";
        error(e.base().what());
      },
      deviceId, ruleId);
}

void RuleRepository::createRule(
    int deviceId, const std::string &op, double value,
    std::function<void(int)> success,
    std::function<void(const std::string &)> error) const {
  LOG_INFO << "[rule_engine][db] rules.createRule transaction: started";
  db_->newTransactionAsync(
      [=](const std::shared_ptr<drogon::orm::Transaction> &trans) {
        if (!trans) {
          LOG_ERROR << "[rule_engine][db] could not start rules.createRule transaction";
          error("Cannot start transaction");
          return;
        }

        LOG_INFO << "[rule_engine][db] rules.insert: started";
        trans->execSqlAsync(
            "INSERT INTO rules (operator, value) "
            "VALUES ($1::rule_operator, $2) RETURNING rule_id",
            [=](const drogon::orm::Result &result) {
              const int ruleId = result[0]["rule_id"].as<int>();
              LOG_INFO << "[rule_engine][db] rules.insert: completed";

              LOG_INFO << "[rule_engine][db] device_to_rule.insert: started";
              trans->execSqlAsync(
                  "INSERT INTO device_to_rule (device_id, rule_id) "
                  "VALUES ($1, $2)",
                  [=](const drogon::orm::Result &) {
                    LOG_INFO << "[rule_engine][db] device_to_rule.insert: completed";
                    success(ruleId);
                  },
                  [=](const drogon::orm::DrogonDbException &e) {
                    trans->rollback();
                    LOG_ERROR << "[rule_engine][db] device_to_rule.insert failed";
                    error(e.base().what());
                  },
                  deviceId, ruleId);
            },
            [=](const drogon::orm::DrogonDbException &e) {
              trans->rollback();
              LOG_ERROR << "[rule_engine][db] rules.insert failed";
              error(e.base().what());
            },
            op, value);
      });
}

void RuleRepository::getRuleById(
    int ruleId, std::function<void(const Rule &)> success,
    std::function<void(const std::string &)> error) const {
  LOG_INFO << "[rule_engine][db] rules.getRuleById: started";
  db_->execSqlAsync(
      "SELECT rule_id, operator, value FROM rules WHERE rule_id = $1",
      [success, error](const drogon::orm::Result &result) {
        LOG_INFO << "[rule_engine][db] rules.getRuleById: completed";
        if (result.empty()) {
          error("Rule not found");
          return;
        }

        Rule rule;
        rule.rule_id = result[0]["rule_id"].as<int>();
        rule.op = result[0]["operator"].as<std::string>();
        rule.value = result[0]["value"].as<double>();
        success(rule);
      },
      [error](const drogon::orm::DrogonDbException &e) {
        LOG_ERROR << "[rule_engine][db] rules.getRuleById failed";
        error(e.base().what());
      },
      ruleId);
}

void RuleRepository::getRulesForDevice(
    int deviceId, std::function<void(const std::vector<Rule> &)> success,
    std::function<void(const std::string &)> error) const {
  LOG_INFO << "[rule_engine][db] rules.getRulesForDevice: started";
  db_->execSqlAsync(
      "SELECT r.rule_id, r.operator, r.value "
      "FROM rules r "
      "JOIN device_to_rule dr ON dr.rule_id = r.rule_id "
      "WHERE dr.device_id = $1",
      [success](const drogon::orm::Result &result) {
        LOG_INFO << "[rule_engine][db] rules.getRulesForDevice: completed";
        std::vector<Rule> rules;
        rules.reserve(result.size());
        for (const auto &row : result) {
          Rule rule;
          rule.rule_id = row["rule_id"].as<int>();
          rule.op = row["operator"].as<std::string>();
          rule.value = row["value"].as<double>();
          rules.push_back(std::move(rule));
        }
        success(rules);
      },
      [error](const drogon::orm::DrogonDbException &e) {
        LOG_ERROR << "[rule_engine][db] rules.getRulesForDevice failed";
        error(e.base().what());
      },
      deviceId);
}

void RuleRepository::updateRule(
    int ruleId, const std::string &op, double value,
    std::function<void()> success,
    std::function<void(const std::string &)> error) const {
  LOG_INFO << "[rule_engine][db] rules.updateRule: started";
  db_->execSqlAsync(
      "UPDATE rules SET operator = $1::rule_operator, value = $2 "
      "WHERE rule_id = $3",
      [success](const drogon::orm::Result &) {
        LOG_INFO << "[rule_engine][db] rules.updateRule: completed";
        success();
      },
      [error](const drogon::orm::DrogonDbException &e) {
        LOG_ERROR << "[rule_engine][db] rules.updateRule failed";
        error(e.base().what());
      },
      op, value, ruleId);
}

void RuleRepository::deleteRule(
    int ruleId, std::function<void()> success,
    std::function<void(const std::string &)> error) const {
  LOG_INFO << "[rule_engine][db] rules.deleteRule: started";
  db_->execSqlAsync(
      "DELETE FROM rules WHERE rule_id = $1",
      [success](const drogon::orm::Result &) {
        LOG_INFO << "[rule_engine][db] rules.deleteRule: completed";
        success();
      },
      [error](const drogon::orm::DrogonDbException &e) {
        LOG_ERROR << "[rule_engine][db] rules.deleteRule failed";
        error(e.base().what());
      },
      ruleId);
}

void RuleRepository::getChatIdForDevice(
    int deviceId, std::function<void(const std::string &)> success,
    std::function<void(const std::string &)> error) const {
  // devices.owner_id -> users.id -> users.telegram_chat_id
  LOG_INFO << "[rule_engine][db] rules.getChatIdForDevice: started";
  db_->execSqlAsync(
      "SELECT u.telegram_chat_id "
      "FROM devices d "
      "JOIN users u ON u.id = d.owner_id "
      "WHERE d.id = $1 AND u.telegram_chat_id IS NOT NULL",
      [success](const drogon::orm::Result &result) {
        LOG_INFO << "[rule_engine][db] rules.getChatIdForDevice: completed";
        if (!result.empty()) {
          const auto value = result[0]["telegram_chat_id"];
          success(value.isNull() ? std::string{} : value.as<std::string>());
        } else {
          success(std::string{});  // Чат не настроен.
        }
      },
      [error](const drogon::orm::DrogonDbException &e) {
        LOG_ERROR << "[rule_engine][db] rules.getChatIdForDevice failed";
        error(e.base().what());
      },
      deviceId);
}
