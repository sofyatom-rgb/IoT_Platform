#pragma once

#include <functional>
#include <string>

/// Контракт операций с устройствами, доступных другим компонентам.
class IDevicesService {
 public:
  /// Позволяет безопасно удалять реализацию через указатель на интерфейс.
  virtual ~IDevicesService() = default;

  /// Создаёт устройство и возвращает ID новой записи.
  virtual void save(int ownerId, const std::string &name,
                    const std::string &type, std::function<void(int)> success,
                    std::function<void(const std::string &)> error) const = 0;

  /// Удаляет устройство по ID.
  virtual void deleteById(int deviceId, std::function<void()> success,
                          std::function<void(const std::string &)> error) const = 0;

  /// Возвращает владельца и имя устройства для уведомления.
  virtual void getNotificationInfo(
      int deviceId, std::function<void(int, const std::string &)> success,
      std::function<void(const std::string &)> error) const = 0;

  /// Обновляет устройство.
  virtual void updateById(int deviceId, int ownerId, const std::string &name,
                          const std::string &type,
                          std::function<void()> success,
                          std::function<void(const std::string &)> error) const = 0;

  /// Возвращает данные устройства в JSON.
  virtual void getById(int deviceId,
                       std::function<void(const std::string &)> success,
                       std::function<void(const std::string &)> error) const = 0;
};
