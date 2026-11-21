#pragma once

#include <functional>
#include <string>

/// Контракт операций с устройствами в хранилище.
class IDevicesRepository {
 public:
  /// Позволяет удалять реализацию через указатель на интерфейс.
  virtual ~IDevicesRepository() = default;

  /// Создаёт устройство и возвращает его ID.
  virtual void save(int ownerId, const std::string &name,
                    const std::string &type, std::function<void(int)> success,
                    std::function<void(const std::string &)> error) const = 0;

  /// Удаляет устройство по ID.
  virtual void deleteById(int deviceId, std::function<void()> success,
                          std::function<void(const std::string &)> error) const = 0;

  /// Возвращает ID владельца и имя устройства для уведомления.
  virtual void getNotificationInfo(
      int deviceId, std::function<void(int, const std::string &)> success,
      std::function<void(const std::string &)> error) const = 0;

  /// Изменяет владельца и параметры устройства.
  virtual void updateById(int deviceId, int ownerId, const std::string &name,
                          const std::string &type,
                          std::function<void()> success,
                          std::function<void(const std::string &)> error) const = 0;

  /// Возвращает данные устройства в JSON.
  virtual void getById(int deviceId,
                       std::function<void(const std::string &)> success,
                       std::function<void(const std::string &)> error) const = 0;
  virtual void getByOwnerId(int, std::function<void(const std::string &)> ,
                            std::function<void(const std::string &)> error) const { error("Device list is unavailable"); }
};
