#include "auth/services/TokenService.h"

#include <jwt-cpp/jwt.h>

#include <chrono>
#include <cstdlib>
#include <stdexcept>

std::string TokenService::create(int userId) {
  auto now = std::chrono::system_clock::now();
  return jwt::create()
      .set_type("JWT")
      .set_subject(std::to_string(userId))
      .set_issued_at(now)
      .set_expires_at(now + std::chrono::minutes(30))
      .sign(jwt::algorithm::hs256{secret()});
}

std::optional<int> TokenService::verify(const std::string &token) {
  try {
    auto decoded = jwt::decode(token);
    jwt::verify()
        .allow_algorithm(jwt::algorithm::hs256{secret()})
        .verify(decoded);
    return std::stoi(decoded.get_subject());
  } catch (...) {
    return std::nullopt;
  }
}

std::string TokenService::secret() {
  const char *s = std::getenv("JWT_SECRET");
  if (!s || std::string(s).size() < 32) {
    throw std::runtime_error("JWT_SECRET is not set or too short");
  }
  return s;
}