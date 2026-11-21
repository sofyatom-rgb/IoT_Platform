#include "PasswordHasher.h"

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include <stdexcept>
#include <utility>

namespace {

constexpr int kIterations = 600000;  // чем больше, тем медленнее перебор
constexpr int kSaltBytes = 16;
constexpr int kHashBytes = 32;

/// Переводит байтовый массив в строчную шестнадцатеричную строку.
std::string toHex(const unsigned char* data, size_t len) {
  static const char* digits = "0123456789abcdef";
  std::string out;
  out.reserve(len * 2);
  for (size_t i = 0; i < len; ++i) {
    out.push_back(digits[data[i] >> 4]);
    out.push_back(digits[data[i] & 0x0F]);
  }
  return out;
}

}  // namespace

PasswordHasher::PasswordHasher(std::string pepper)
    : pepper_(std::move(pepper)) {}

std::string PasswordHasher::generateSalt() {
  unsigned char buf[kSaltBytes];
  if (RAND_bytes(buf, sizeof(buf)) != 1) {
    throw std::runtime_error("RAND_bytes failed");
  }
  return toHex(buf, sizeof(buf));
}

std::string PasswordHasher::hash(const std::string& password,
                                 const std::string& salt) const {
  const std::string input = password + pepper_;
  unsigned char out[kHashBytes];
  int ok = PKCS5_PBKDF2_HMAC(
      input.c_str(), static_cast<int>(input.size()),
      reinterpret_cast<const unsigned char*>(salt.data()),
      static_cast<int>(salt.size()), kIterations, EVP_sha256(), kHashBytes,
      out);
  if (ok != 1) {
    throw std::runtime_error("PBKDF2 failed");
  }
  return toHex(out, kHashBytes);
}

bool PasswordHasher::verify(const std::string& password,
                            const std::string& salt,
                            const std::string& expectedHash) const {
  const std::string actual = hash(password, salt);
  if (actual.size() != expectedHash.size()) return false;
  // Постоянное время: нельзя выдавать, на каком символе хеши разошлись
  return CRYPTO_memcmp(actual.data(), expectedHash.data(), actual.size()) == 0;
}