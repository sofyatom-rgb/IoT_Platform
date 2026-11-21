#pragma once

#include <string>

namespace codec {

/// Кодирует строку в Base64 по RFC 4648.
std::string base64Encode(const std::string &data);

/// Декодирует Base64; при ошибке возвращает false.
bool base64Decode(const std::string &text, std::string &out);

}  // namespace codec
