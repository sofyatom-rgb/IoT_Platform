#pragma once

#include <cstdint>
#include <string>

namespace codec {

// CRC32 (IEEE 802.3, полином 0xEDB88320).
std::uint32_t crc32(const std::string &data);
std::string   crc32Hex(const std::string &data);  // 8 hex-символов, lowercase

}  // namespace codec