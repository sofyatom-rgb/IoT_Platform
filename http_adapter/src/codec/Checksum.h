#pragma once

#include <cstdint>
#include <string>

namespace codec {

/// Вычисляет CRC32 (IEEE 802.3) для строки.
std::uint32_t crc32(const std::string &data);

/// Возвращает CRC32 в виде восьми строчных hex-символов.
std::string crc32Hex(const std::string &data);

}  // namespace codec
