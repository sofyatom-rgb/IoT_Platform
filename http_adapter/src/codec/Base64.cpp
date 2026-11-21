#include "codec/Base64.h"

#include <array>
#include <cstdint>

namespace codec {
namespace {

const char kAlphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::array<int8_t, 256> makeDecodeTable() {
  std::array<int8_t, 256> t{};
  t.fill(-1);
  for (int i = 0; i < 64; ++i) {
    t[static_cast<unsigned char>(kAlphabet[i])] = static_cast<int8_t>(i);
  }
  return t;
}
const std::array<int8_t, 256> kDecodeTable = makeDecodeTable();

}  // namespace

std::string base64Encode(const std::string &data) {
  std::string out;
  out.reserve(((data.size() + 2) / 3) * 4);

  std::size_t i = 0;
  while (i + 3 <= data.size()) {
    std::uint32_t v = (static_cast<std::uint8_t>(data[i]) << 16) |
                      (static_cast<std::uint8_t>(data[i + 1]) << 8) |
                      static_cast<std::uint8_t>(data[i + 2]);
    out.push_back(kAlphabet[(v >> 18) & 0x3F]);
    out.push_back(kAlphabet[(v >> 12) & 0x3F]);
    out.push_back(kAlphabet[(v >> 6) & 0x3F]);
    out.push_back(kAlphabet[v & 0x3F]);
    i += 3;
  }

  const std::size_t rem = data.size() - i;
  if (rem == 1) {
    std::uint32_t v = static_cast<std::uint8_t>(data[i]) << 16;
    out.push_back(kAlphabet[(v >> 18) & 0x3F]);
    out.push_back(kAlphabet[(v >> 12) & 0x3F]);
    out.push_back('=');
    out.push_back('=');
  } else if (rem == 2) {
    std::uint32_t v = (static_cast<std::uint8_t>(data[i]) << 16) |
                      (static_cast<std::uint8_t>(data[i + 1]) << 8);
    out.push_back(kAlphabet[(v >> 18) & 0x3F]);
    out.push_back(kAlphabet[(v >> 12) & 0x3F]);
    out.push_back(kAlphabet[(v >> 6) & 0x3F]);
    out.push_back('=');
  }
  return out;
}

bool base64Decode(const std::string &text, std::string &out) {
  out.clear();
  if (text.size() % 4 != 0) return false;
  out.reserve((text.size() / 4) * 3);

  for (std::size_t i = 0; i < text.size(); i += 4) {
    int vals[4];
    int pad = 0;
    for (int j = 0; j < 4; ++j) {
      const char c = text[i + j];
      if (c == '=') {
        vals[j] = 0;
        ++pad;
        if (j < 2) return false;  // '=' не может стоять в первых двух позициях
      } else {
        if (pad > 0) return false;  // символ после '='
        const int8_t v = kDecodeTable[static_cast<unsigned char>(c)];
        if (v < 0) return false;
        vals[j] = v;
      }
    }
    const std::uint32_t v = (vals[0] << 18) | (vals[1] << 12) |
                            (vals[2] << 6) | vals[3];
    out.push_back(static_cast<char>((v >> 16) & 0xFF));
    if (pad < 2) out.push_back(static_cast<char>((v >> 8) & 0xFF));
    if (pad < 1) out.push_back(static_cast<char>(v & 0xFF));
  }
  return true;
}

}  // namespace codec