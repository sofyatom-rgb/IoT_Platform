#pragma once

#include <string>

namespace codec {

/// Проверяет контрольную сумму и расшифровывает конверт AES-CBC.
class Decoder {
 public:
  /// Создаёт декодер с ключом AES-256.
  explicit Decoder(std::string key);

  /// Расшифровывает JSON-конверт; ошибку возвращает через error.
  bool decode(const std::string &envelopeJson, std::string &plaintext,
              std::string &error) const;

 private:
  std::string key_;
};

}  // namespace codec
