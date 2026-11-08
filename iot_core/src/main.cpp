#include <drogon/drogon.h>

#include <fstream>
#include <filesystem>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "common/http/DrogonHttpTransport.h"
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
  const auto configDirectory =
      std::filesystem::absolute(argv[0]).parent_path().parent_path() / "config";
  auto config = readConfig(configDirectory / "HTTPConfig.json");
  const auto databaseConfig =
      readConfig(configDirectory / "PostgreSQLConfig.json");
  if (!databaseConfig["db_clients"].isArray() ||
      databaseConfig["db_clients"].empty()) {
    std::cerr << "PostgreSQLConfig.json must define a non-empty db_clients "
                 "array\n";
    return 1;
  }
  const auto &database = databaseConfig["db_clients"][0];
  if (database.get("rdbms", "postgresql").asString() != "postgresql" ||
      !database["host"].isString() || !database["port"].isUInt() ||
      !database["dbname"].isString() || !database["user"].isString() ||
      !database["passwd"].isString()) {
    std::cerr << "The first db_clients entry must contain valid PostgreSQL "
                 "connection settings\n";
    return 1;
  }

  std::ostringstream connectionInfo;
  connectionInfo << "host=" << quoteConnectionValue(database["host"].asString())
                 << " port=" << database["port"].asUInt()
                 << " dbname=" << quoteConnectionValue(database["dbname"].asString())
                 << " user=" << quoteConnectionValue(database["user"].asString())
                 << " password=" << quoteConnectionValue(database["passwd"].asString());
  drogon::app().loadConfigJson(config);

  const auto &customConfig = drogon::app().getCustomConfig();
  if (!customConfig["rule_engine_url"].isString() ||
      customConfig["rule_engine_url"].asString().empty()) {
    std::cerr << "HTTPConfig.json must define rule_engine_url under "
                 "custom_config\n";
    return 1;
  }

  // Composition root: здесь создаются реальные реализации зависимостей.
  const auto connectionCount =
      database.get("number_of_connections", 1).asUInt();
  if (connectionCount == 0) {
    std::cerr << "PostgreSQL connection count must be greater than zero\n";
    return 1;
  }
  const auto db =
      drogon::orm::DbClient::newPgClient(connectionInfo.str(), connectionCount);
  if (database["timeout"].isNumeric()) {
    db->setTimeout(database["timeout"].asDouble());
  }
  auto usersService = std::make_shared<UsersService>(
      std::make_shared<UsersRepository>(db));
  auto devicesService = std::make_shared<DevicesService>(
      std::make_shared<DevicesRepository>(db));
  auto ruleEngineClient = std::make_shared<RuleEngineClient>(
      customConfig["rule_engine_url"].asString(),
      std::make_shared<iot::http::DrogonHttpTransport>(
          customConfig["rule_engine_url"].asString()));
  auto telemetryService = std::make_shared<TelemetryService>(
      std::make_shared<TelemetryRepository>(db),
      ruleEngineClient);

  // Контроллеры получают готовые сервисы через конструкторы.
  TelemetryController telemetryController(telemetryService);
  telemetryController.registerRoutes();

  UsersController usersController(usersService);
  usersController.registerRoutes();

  DevicesController devicesController(devicesService);
  devicesController.registerRoutes();

  drogon::app().run();
  return 0;
}
