#pragma once

#include <functional>
#include <string>

class IUsersService {
 public:
  virtual ~IUsersService() = default;
  virtual void save(const std::string &, const std::string &, const std::string &,
                    const std::string &, std::function<void(int)>,
                    std::function<void(const std::string &)>) const = 0;
  virtual void deleteById(int, std::function<void()>,
                          std::function<void(const std::string &)>) const = 0;
  virtual void getTelegramChatId(int, std::function<void(const std::string &)>,
                                 std::function<void(const std::string &)>) const = 0;
  virtual void updateById(int, const std::string &, const std::string &,
                          const std::string &, const std::string &,
                          std::function<void()>,
                          std::function<void(const std::string &)>) const = 0;
  virtual void getById(int, std::function<void(const std::string &)>,
                       std::function<void(const std::string &)>) const = 0;
};
