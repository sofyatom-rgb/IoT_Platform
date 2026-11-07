#include "codec/Base64.h"

#include <array>
#include <cstdint>
#include <utility>

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

  std::string decoded;
  decoded.reserve((text.size() / 4) * 3);

  for (std::size_t i = 0; i < text.size(); i += 4) {
    const bool isLastBlock = (i + 4 == text.size());
    const char c0 = text[i];
    const char c1 = text[i + 1];
    const char c2 = text[i + 2];
    const char c3 = text[i + 3];

    // Первые два символа всегда являются данными.
    if (c0 == '=' || c1 == '=') return false;

    const int8_t v0 = kDecodeTable[static_cast<unsigned char>(c0)];
    const int8_t v1 = kDecodeTable[static_cast<unsigned char>(c1)];
    if (v0 < 0 || v1 < 0) return false;

    const bool pad2 = c2 == '=';
    const bool pad1 = c3 == '=';
    if (pad2 && !pad1) return false;       // допустимы только "xx==" или "xxx=".
    if ((pad1 || pad2) && !isLastBlock) return false;  // padding только в конце.

    const int8_t v2 = pad2 ? 0 : kDecodeTable[static_cast<unsigned char>(c2)];
    const int8_t v3 = pad1 ? 0 : kDecodeTable[static_cast<unsigned char>(c3)];
    if ((!pad2 && v2 < 0) || (!pad1 && v3 < 0)) return false;

    // RFC 4648: неиспользуемые младшие биты последнего символа должны быть нулевыми.
    if (pad2 && (v1 & 0x0F) != 0) return false;
    if (!pad2 && pad1 && (v2 & 0x03) != 0) return false;

    const std::uint32_t value =
        (static_cast<std::uint32_t>(v0) << 18) |
        (static_cast<std::uint32_t>(v1) << 12) |
        (static_cast<std::uint32_t>(v2) << 6) |
        static_cast<std::uint32_t>(v3);

    decoded.push_back(static_cast<char>((value >> 16) & 0xFF));
    if (!pad2) decoded.push_back(static_cast<char>((value >> 8) & 0xFF));
    if (!pad1) decoded.push_back(static_cast<char>(value & 0xFF));
  }

  out = std::move(decoded);
  return true;
}

}  // namespace codec