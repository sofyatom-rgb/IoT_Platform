#pragma once

#include <string>

namespace codec {

/// Шифрует текст и упаковывает его вместе с CRC32 в JSON-конверт.
class Encoder {
 public:
  /// Создаёт кодировщик; ключ приводится к 32 байтам для AES-256.
  explicit Encoder(std::string key);

  /// Возвращает JSON-конверт с шифротекстом и контрольной суммой.
  std::string encode(const std::string &plaintext) const;

 private:
  std::string key_;
};

}  // namespace codec
