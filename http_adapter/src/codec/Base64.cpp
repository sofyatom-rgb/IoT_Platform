#include "codec/Base64.h"

#include <array>
#include <cstdint>

namespace codec {
namespace {

const char kAlphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Создаёт таблицу преобразования символов Base64 в шестибитные значения.
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

  const auto invalid = [&out]() {
    out.clear();
    return false;
  };

  for (std::size_t i = 0; i < text.size(); i += 4) {
    const bool isLastBlock = i + 4 == text.size();
    const bool padThird = text[i + 2] == '=';
    const bool padFourth = text[i + 3] == '=';

    if (text[i] == '=' || text[i + 1] == '=' ||
        (!isLastBlock && (padThird || padFourth)) ||
        (padThird && !padFourth)) {
      return invalid();
    }

    const int8_t first = kDecodeTable[static_cast<unsigned char>(text[i])];
    const int8_t second = kDecodeTable[static_cast<unsigned char>(text[i + 1])];
    if (first < 0 || second < 0) return invalid();

    int third = 0;
    int fourth = 0;
    if (!padThird) {
      third = kDecodeTable[static_cast<unsigned char>(text[i + 2])];
      if (third < 0) return invalid();
    }
    if (!padFourth) {
      fourth = kDecodeTable[static_cast<unsigned char>(text[i + 3])];
      if (fourth < 0) return invalid();
    }

    // Отбрасываем блоки с ненулевыми битами, скрытыми padding-символами.
    if ((padThird && (second & 0x0F) != 0) ||
        (padFourth && !padThird && (third & 0x03) != 0)) {
      return invalid();
    }

    const std::uint32_t value =
        (static_cast<std::uint32_t>(first) << 18) |
        (static_cast<std::uint32_t>(second) << 12) |
        (static_cast<std::uint32_t>(third) << 6) |
        static_cast<std::uint32_t>(fourth);

    out.push_back(static_cast<char>((value >> 16) & 0xFF));
    if (!(padThird && padFourth)) {
      out.push_back(static_cast<char>((value >> 8) & 0xFF));
    }
    if (!padThird && !padFourth) out.push_back(static_cast<char>(value & 0xFF));
  }
  return true;
}

}  // namespace codec