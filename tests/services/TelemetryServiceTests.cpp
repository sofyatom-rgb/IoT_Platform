#include "TestFramework.h"

#include "telemetry/services/TelemetryService.h"

#include <memory>
#include <stdexcept>

namespace {
class FakeTelemetryRepository final : public ITelemetryRepository {
 public:
  mutable int deviceId{-1};
  mutable double temperature{0.0};
  mutable std::string failure;
  mutable int saves{0}, reads{0}, deletes{0};
  mutable bool deferSave{false};
  mutable std::function<void()> pendingSaveSuccess;
  mutable std::function<void(const std::string &)> pendingSaveError;

  void save(int id, double value, std::function<void()> success,
            std::function<void(const std::string &)> error) const override {
    deviceId = id;
    temperature = value;
    ++saves;
    if (deferSave) {
      pendingSaveSuccess = std::move(success);
      pendingSaveError = std::move(error);
    } else if (failure.empty()) {
      success();
    } else {
      error(failure);
    }
  }

  void completeDeferredSave() const {
    if (failure.empty()) {
      if (pendingSaveSuccess) pendingSaveSuccess();
    } else if (pendingSaveError) {
      pendingSaveError(failure);
    }
  }

  void getByDeviceId(int id, std::function<void(const std::string &)> success,
                     std::function<void(const std::string &)> error) const override {
    deviceId = id;
    ++reads;
    if (failure.empty()) success("[]");
    else error(failure);
  }

  void deleteByDeviceId(int id, std::function<void()> success,
                        std::function<void(const std::string &)> error) const override {
    deviceId = id;
    ++deletes;
    if (failure.empty()) success();
    else error(failure);
  }
};

class FakeRuleEngineClient final : public IRuleEngineClient {
 public:
  mutable int deviceId{-1};
  mutable double temperature{0.0};
  mutable int calls{0};
  mutable std::string status{"not_triggered"};
  mutable std::string failure;

  void checkTelemetry(int id, double value,
                      std::function<void(const std::string &)> success,
                      std::function<void(const std::string &)> error) const override {
    ++calls;
    deviceId = id;
    temperature = value;
    if (failure.empty()) success(status);
    else error(failure);
  }
};

struct Fixture {
  std::shared_ptr<FakeTelemetryRepository> repository =
      std::make_shared<FakeTelemetryRepository>();
  std::shared_ptr<FakeRuleEngineClient> ruleEngine =
      std::make_shared<FakeRuleEngineClient>();
  TelemetryService service{repository, ruleEngine};
};
}  // namespace

TEST_CASE(telemetry_saves_then_forwards_reading_to_rule_engine) {
  Fixture f;
  f.ruleEngine->status = "telegram=sent; email=sent";
  std::string result;

  f.service.save(12, 41.75, [&](const std::string &value) { result = value; },
                 [](const std::string &) { EXPECT_TRUE(false); });

  EXPECT_EQ("telegram=sent; email=sent", result);
  EXPECT_EQ(12, f.repository->deviceId);
  EXPECT_EQ(41.75, f.repository->temperature);
  EXPECT_EQ(1, f.repository->saves);
  EXPECT_EQ(12, f.ruleEngine->deviceId);
  EXPECT_EQ(41.75, f.ruleEngine->temperature);
  EXPECT_EQ(1, f.ruleEngine->calls);
}

TEST_CASE(telemetry_does_not_call_rule_engine_when_save_fails) {
  Fixture f;
  f.repository->failure = "database unavailable";
  std::string error;
  f.service.save(7, 55.0, [](const std::string &) { EXPECT_TRUE(false); },
                 [&](const std::string &value) { error = value; });

  EXPECT_EQ("database unavailable", error);
  EXPECT_EQ(0, f.ruleEngine->calls);
}

TEST_CASE(telemetry_propagates_rule_engine_failure_after_persisting) {
  Fixture f;
  f.ruleEngine->failure = "HTTP 503";
  std::string error;
  f.service.save(4, 40.0, [](const std::string &) { EXPECT_TRUE(false); },
                 [&](const std::string &value) { error = value; });

  EXPECT_EQ(1, f.repository->saves);
  EXPECT_EQ("HTTP 503", error);
}

TEST_CASE(telemetry_waits_for_persistence_before_calling_rule_engine) {
  Fixture f;
  f.repository->deferSave = true;
  int completionCalls = 0;

  f.service.save(18, 31.0,
                 [&](const std::string &) { ++completionCalls; },
                 [](const std::string &) { EXPECT_TRUE(false); });
  EXPECT_EQ(0, f.ruleEngine->calls);
  EXPECT_EQ(0, completionCalls);

  f.repository->completeDeferredSave();
  EXPECT_EQ(1, f.ruleEngine->calls);
  EXPECT_EQ(1, completionCalls);
}

TEST_CASE(telemetry_read_and_delete_are_forwarded_to_repository) {
  Fixture f;
  std::string body;
  f.service.getByDeviceId(14, [&](const std::string &value) { body = value; },
                          [](const std::string &) { EXPECT_TRUE(false); });
  EXPECT_EQ("[]", body);
  EXPECT_EQ(14, f.repository->deviceId);
  EXPECT_EQ(1, f.repository->reads);

  bool deleted = false;
  f.service.deleteByDeviceId(14, [&] { deleted = true; },
                             [](const std::string &) { EXPECT_TRUE(false); });
  EXPECT_TRUE(deleted);
  EXPECT_EQ(1, f.repository->deletes);
}

TEST_CASE(telemetry_repository_only_service_rejects_save_without_rule_engine) {
  auto repository = std::make_shared<FakeTelemetryRepository>();
  TelemetryService service(repository);
  std::string error;
  service.save(5, 99.0, [](const std::string &) { EXPECT_TRUE(false); },
               [&](const std::string &value) { error = value; });

  EXPECT_EQ("TelemetryService Rule Engine client is not configured", error);
  EXPECT_EQ(0, repository->saves);
}

TEST_CASE(telemetry_constructor_rejects_null_dependencies) {
  bool repositoryRejected = false;
  try {
    TelemetryService service(nullptr);
  } catch (const std::invalid_argument &) {
    repositoryRejected = true;
  }
  EXPECT_TRUE(repositoryRejected);

  bool ruleEngineRejected = false;
  try {
    TelemetryService service(std::make_shared<FakeTelemetryRepository>(), nullptr);
  } catch (const std::invalid_argument &) {
    ruleEngineRejected = true;
  }
  EXPECT_TRUE(ruleEngineRejected);
}
