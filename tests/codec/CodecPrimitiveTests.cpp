#include "TestFramework.h"

#include "codec/Base64.h"
#include "codec/Checksum.h"

#include <string>
#include <utility>
#include <vector>

TEST_CASE(base64_rfc4648_vectors_and_binary_data) {
  const std::vector<std::pair<std::string, std::string>> cases = {
      {"", ""}, {"f", "Zg=="}, {"fo", "Zm8="}, {"foo", "Zm9v"},
      {"foobar", "Zm9vYmFy"}, {std::string("a\0b", 3), "YQBi"}};
  for (const auto &[plain, expected] : cases) {
    EXPECT_EQ(expected, codec::base64Encode(plain));
    std::string decoded;
    EXPECT_TRUE(codec::base64Decode(expected, decoded));
    EXPECT_EQ(plain, decoded);
  }
}

TEST_CASE(base64_rejects_invalid_padding_and_clears_partial_output) {
  const std::vector<std::string> invalid = {
      "abc", "!!!!", "=AAA", "AA=A", "A===", "AA==AAAA",
      "AAAA====", "AB==", "AAB="};
  for (const auto &value : invalid) {
    std::string decoded = "stale value";
    EXPECT_FALSE(codec::base64Decode(value, decoded));
    EXPECT_EQ("", decoded);
  }
  std::string decoded = "stale value";
  EXPECT_FALSE(codec::base64Decode("Zm9v!!!!", decoded));
  EXPECT_EQ("", decoded);
}

TEST_CASE(crc32_matches_ieee_reference_values) {
  EXPECT_EQ(0xCBF43926u, codec::crc32("123456789"));
  EXPECT_EQ(std::string("cbf43926"), codec::crc32Hex("123456789"));
  EXPECT_EQ(0u, codec::crc32(""));
  EXPECT_EQ(std::string("00000000"), codec::crc32Hex(""));
}

TEST_CASE(base64_round_trips_various_lengths_with_binary_bytes) {
  for (std::size_t length = 0; length <= 256; ++length) {
    std::string payload;
    payload.reserve(length);
    for (std::size_t i = 0; i < length; ++i) {
      payload.push_back(static_cast<char>((i * 37 + length * 13) & 0xFF));
    }
    const auto encoded = codec::base64Encode(payload);
    std::string decoded;
    EXPECT_TRUE(codec::base64Decode(encoded, decoded));
    EXPECT_EQ(payload, decoded);
  }
}
