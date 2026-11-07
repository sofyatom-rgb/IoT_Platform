#pragma once

#include <functional>
#include <string>
#include <utility>

#include "users/repositories/IUsersRepository.h"

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
  virtual void getNotificationTargets(
      int userId, std::function<void(const NotificationTargets &)> success,
      std::function<void(const std::string &)> error) const {
    getTelegramChatId(
        userId,
        [success = std::move(success)](const std::string &chatId) {
          success(NotificationTargets{chatId, {}});
        },
        std::move(error));
  }
  virtual void updateById(int, const std::string &, const std::string &,
                          const std::string &, const std::string &,
                          std::function<void()>,
                          std::function<void(const std::string &)>) const = 0;
  virtual void getById(int, std::function<void(const std::string &)>,
                       std::function<void(const std::string &)>) const = 0;
};
