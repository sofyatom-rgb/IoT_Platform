#pragma once

#include <functional>
#include <string>

class IDevicesService {
 public:
  virtual ~IDevicesService() = default;
  virtual void save(int, const std::string &, const std::string &,
                    std::function<void(int)>,
                    std::function<void(const std::string &)>) const = 0;
  virtual void deleteById(int, std::function<void()>,
                          std::function<void(const std::string &)>) const = 0;
  virtual void getNotificationInfo(int,
                                   std::function<void(int, const std::string &)>,
                                   std::function<void(const std::string &)>) const = 0;
  virtual void updateById(int, int, const std::string &, const std::string &,
                          std::function<void()>,
                          std::function<void(const std::string &)>) const = 0;
  virtual void getById(int, std::function<void(const std::string &)>,
                       std::function<void(const std::string &)>) const = 0;
};
