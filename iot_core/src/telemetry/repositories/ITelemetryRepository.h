#pragma once

#include <functional>
#include <string>

/// Контракт чтения и записи телеметрии.
class ITelemetryRepository {
 public:
  /// Позволяет удалять реализацию через указатель на интерфейс.
  virtual ~ITelemetryRepository() = default;

  /// Сохраняет показание температуры.
  virtual void save(int deviceId, double temperature,
                    std::function<void()> success,
                    std::function<void(const std::string &)> error) const = 0;

  /// Возвращает последние показания устройства в JSON.
  virtual void getByDeviceId(
      int deviceId, std::function<void(const std::string &)> success,
      std::function<void(const std::string &)> error) const = 0;

  /// Удаляет телеметрию устройства.
  virtual void deleteByDeviceId(
      int deviceId, std::function<void()> success,
      std::function<void(const std::string &)> error) const = 0;
};
