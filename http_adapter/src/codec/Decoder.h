#pragma once

#include <string>

namespace codec {

class Decoder {
 public:
  explicit Decoder(std::string key);

  // Проверяет checksum и расшифровывает конверт.
  // Возвращает true и заполняет plaintext, либо false и заполняет error.
  bool decode(const std::string &envelopeJson,
              std::string &plaintext,
              std::string &error) const;

 private:
  std::string key_;
};

}  // namespace codec