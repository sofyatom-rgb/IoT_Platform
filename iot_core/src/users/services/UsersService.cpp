#include "users/services/UsersService.h"

#include <stdexcept>
#include <utility>

UsersService::UsersService(std::shared_ptr<IUsersRepository> repository)
    : repository_(std::move(repository)) {
  if (!repository_) throw std::invalid_argument("UsersService requires a repository");
}

void UsersService::save(const std::string &email, const std::string &password,
                        const std::string &name, const std::string &telegramChatId,
                        std::function<void(int)> success,
                        std::function<void(const std::string &)> error) const {
  repository_->save(email, password, name, telegramChatId,
                    std::move(success), std::move(error));
}

void UsersService::deleteById(int userId, std::function<void()> success,
                              std::function<void(const std::string &)> error) const {
  repository_->deleteById(userId, std::move(success), std::move(error));
}

void UsersService::getTelegramChatId(
    int userId, std::function<void(const std::string &)> success,
    std::function<void(const std::string &)> error) const {
  repository_->getTelegramChatId(userId, std::move(success), std::move(error));
}

void UsersService::updateById(
    int userId, const std::string &email, const std::string &password,
    const std::string &name, const std::string &telegramChatId,
    std::function<void()> success,
    std::function<void(const std::string &)> error) const {
  repository_->updateById(userId, email, password, name, telegramChatId,
                          std::move(success), std::move(error));
}

void UsersService::getById(int userId,
                           std::function<void(const std::string &)> success,
                           std::function<void(const std::string &)> error) const {
  repository_->getById(userId, std::move(success), std::move(error));
}
