#pragma once

#include <fakeit.hpp>

#include <memory>

namespace test {

// Возвращает интерфейсный указатель на FakeIt-мок и продлевает жизнь самого мока.
template <typename Interface>
std::shared_ptr<Interface> sharedMock(
    const std::shared_ptr<fakeit::Mock<Interface>> &mock) {
  return std::shared_ptr<Interface>(mock, &mock->get());
}

}  // namespace test
