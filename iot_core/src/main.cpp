#include <drogon/drogon.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "auth/controllers/AuthController.h"
#include "common/db/DbClientProvider.h"
#include "common/http/DrogonHttpTransport.h"
#include "common/logging/DrogonLogging.h"
#include "devices/controllers/DevicesController.h"
#include "devices/repositories/DevicesRepository.h"
#include "devices/services/DevicesService.h"
#include "telemetry/clients/RuleEngineClient.h"
#include "telemetry/controllers/TelemetryController.h"
#include "telemetry/repositories/TelemetryRepository.h"
#include "telemetry/services/TelemetryService.h"
#include "users/controllers/UsersController.h"
#include "users/repositories/UsersRepository.h"
#include "users/services/UsersService.h"

namespace {
void loadDotEnv(const std::filesystem::path &path) {
  std::ifstream input(path);
  if (!input) return;
  std::string line;
  while (std::getline(input, line)) {
    if (line.rfind("export ", 0) == 0) line.erase(0, 7);
    if (line.empty() || line[0] == '#') continue;
    const auto equal = line.find('=');
    if (equal == std::string::npos) continue;
    std::string key = line.substr(0, equal);
    std::string value = line.substr(equal + 1);
    while (!key.empty() && (key.front() == ' ' || key.front() == '\t')) key.erase(0, 1);
    while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
    if (!value.empty() && (value.front() == '\'' || value.front() == '"')) {
      const char quote = value.front();
      value.erase(0, 1);
      if (!value.empty() && value.back() == quote) value.pop_back();
    }
    if (!key.empty() && std::getenv(key.c_str()) == nullptr) {
      setenv(key.c_str(), value.c_str(), 1);
    }
  }
}

Json::Value readConfig(const std::filesystem::path &path) {
  std::ifstream input(path);
  if (!input) {
    throw std::runtime_error("Could not open config file: " + path.string());
  }
  Json::CharReaderBuilder builder;
  Json::Value config;
  std::string errors;
  if (!Json::parseFromStream(builder, input, &config, &errors)) {
    throw std::runtime_error("Could not parse config file " + path.string() +
                             ": " + errors);
  }
  return config;
}

std::string quoteConnectionValue(const std::string &value) {
  std::string quoted = "'";
  for (const char character : value) {
    if (character == '\\' || character == '\'') {
      quoted += '\\';
    }
    quoted += character;
  }
  quoted += '\'';
  return quoted;
}
}  // namespace

int main(int, char *argv[]) {
  loadDotEnv(".env");
  loadDotEnv(
      std::filesystem::absolute(argv[0]).parent_path().parent_path().parent_path() / ".env");

  const auto configDirectory =
      std::filesystem::absolute(argv[0]).parent_path().parent_path() / "config";
  auto config = readConfig(configDirectory / "HTTPConfig.json");
  const auto databaseConfig =
      readConfig(configDirectory / "PostgreSQLConfig.json");

  // HTTPConfig.json содержит только HTTP-настройки; DbClient создаётся явно
  // ниже из PostgreSQLConfig.json и сохраняется в DbClientProvider.
  drogon::app().loadConfigFile((configDirectory / "HTTPConfig.json").string());
  iot::logging::configureDrogonLogging(configDirectory / "logs", "iot_core");
  LOG_INFO << "[iot_core] Starting IoT Core";

  const char *pepper = std::getenv("PASSWORD_PEPPER");
  if (!pepper || std::string(pepper).empty()) {
    LOG_ERROR << "[iot_core] PASSWORD_PEPPER is not set";
    return 1;
  }

  const char *jwtSecret = std::getenv("JWT_SECRET");
  if (!jwtSecret || std::string(jwtSecret).size() < 32) {
    LOG_ERROR << "[iot_core] JWT_SECRET is not set or shorter than 32 characters";
    return 1;
  }

  if (!databaseConfig["db_clients"].isArray() ||
      databaseConfig["db_clients"].empty()) {
    LOG_ERROR << "[iot_core] PostgreSQLConfig.json must define a non-empty "
                 "db_clients array";
    return 1;
  }
  const auto &database = databaseConfig["db_clients"][0];
  if (database.get("rdbms", "postgresql").asString() != "postgresql" ||
      !database["host"].isString() || !database["port"].isUInt() ||
      !database["dbname"].isString() || !database["user"].isString() ||
      !database["passwd"].isString()) {
    LOG_ERROR << "[iot_core] The first db_clients entry must contain valid "
                 "PostgreSQL connection settings";
    return 1;
  }

  const auto &customConfig = config["custom_config"];
  if (!customConfig["rule_engine_url"].isString() ||
      customConfig["rule_engine_url"].asString().empty()) {
    LOG_ERROR << "[iot_core] HTTPConfig.json must define rule_engine_url under "
                 "custom_config";
    return 1;
  }

  std::ostringstream connectionInfo;
  connectionInfo << "host=" << quoteConnectionValue(database["host"].asString())
                 << " port=" << database["port"].asUInt()
                 << " dbname=" << quoteConnectionValue(database["dbname"].asString())
                 << " user=" << quoteConnectionValue(database["user"].asString())
                 << " password=" << quoteConnectionValue(database["passwd"].asString());
  const auto connectionCount =
      database.get("number_of_connections", 1).asUInt();
  auto db = drogon::orm::DbClient::newPgClient(connectionInfo.str(), connectionCount);
  if (database["timeout"].isNumeric()) {
    db->setTimeout(database["timeout"].asDouble());
  }
  DbClientProvider::set(db);
  LOG_INFO << "[iot_core] PostgreSQL client configured";

  auto ruleEngineClient = std::make_shared<RuleEngineClient>(
      customConfig["rule_engine_url"].asString(),
      std::make_shared<iot::http::DrogonHttpTransport>(
          customConfig["rule_engine_url"].asString()));
  auto telemetryService = std::make_shared<TelemetryService>(
      std::make_shared<TelemetryRepository>(db),
      ruleEngineClient);

  TelemetryController telemetryController(telemetryService);
  telemetryController.registerRoutes();

  UsersController usersController;
  usersController.registerRoutes();

  DevicesController devicesController;
  devicesController.registerRoutes();

  AuthController authController;
  authController.registerRoutes();

  LOG_INFO << "[iot_core] HTTP server is ready";
  drogon::app().run();
  return 0;
}