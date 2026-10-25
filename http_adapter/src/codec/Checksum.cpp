#include "codec/Checksum.h"

#include <array>

namespace codec {
namespace {

std::array<std::uint32_t, 256> makeTable() {
  std::array<std::uint32_t, 256> t{};
  for (std::uint32_t i = 0; i < 256; ++i) {
    std::uint32_t c = i;
    for (int k = 0; k < 8; ++k) {
      c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
    }
    t[i] = c;
  }
  return t;
}
const std::array<std::uint32_t, 256> kTable = makeTable();

}  // namespace

std::uint32_t crc32(const std::string &data) {
  std::uint32_t crc = 0xFFFFFFFFu;
  for (unsigned char c : data) {
    crc = kTable[(crc ^ c) & 0xFF] ^ (crc >> 8);
  }
  return crc ^ 0xFFFFFFFFu;
}

std::string crc32Hex(const std::string &data) {
  static const char *hex = "0123456789abcdef";
  std::uint32_t v = crc32(data);
  std::string out(8, '0');
  for (int i = 7; i >= 0; --i) {
    out[i] = hex[v & 0xFu];
    v >>= 4;
  }
  return out;
}

}  // namespace codec