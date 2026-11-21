#pragma once

#include <fakeit.hpp>

#include <memory>

namespace test {

/// Передаёт FakeIt-мок через интерфейс, сохраняя его время жизни.
template <typename Interface>
std::shared_ptr<Interface> sharedMock(
    const std::shared_ptr<fakeit::Mock<Interface>> &mock) {
  return std::shared_ptr<Interface>(mock, &mock->get());
}

}  // namespace test
