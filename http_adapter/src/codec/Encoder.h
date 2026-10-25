#pragma once

#include <string>

namespace codec {

// Кодирует произвольный plaintext в конверт
//   {"checksum":"<crc32 hex>","ciphertext":"<base64(iv||AES-CBC(plaintext))>"}
class Encoder {
 public:
  // key приводится к 32 байтам (AES-256): дополняется '\0' или обрезается.
  explicit Encoder(std::string key);

  std::string encode(const std::string &plaintext) const;

 private:
  std::string key_;
};

}  // namespace codec