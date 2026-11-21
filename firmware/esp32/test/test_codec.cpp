#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "codec.h"

namespace {

int failures = 0;

void check(bool condition, const char* expression, int line) {
  if (!condition) {
    std::printf("FAIL line %d: %s\n", line, expression);
    ++failures;
  }
}

#define CHECK(expression) check((expression), #expression, __LINE__)

const uint8_t* testKey() {
  return reinterpret_cast<const uint8_t*>("testkey0123456789abcdef012345678");
}

const uint8_t* wrongKey() {
  return reinterpret_cast<const uint8_t*>("wrongkey0123456789abcdef01234567");
}

const uint8_t kFixedIv[16] = {0, 1, 2, 3, 4,  5,  6,  7,
                              8, 9, 10, 11, 12, 13, 14, 15};

const char* kPlaintext = "{\"device_id\":123,\"temperature\":23.75}";
const char* kExpectedEnvelope =
    "{\"checksum\":\"490570fe\",\"ciphertext\":"
    "\"AAECAwQFBgcICQoLDA0ODx4nsBPs4QQ7HmmrQGXg32jp7hAwdyrMbsO+VOJE5QJ6gG"
    "CyiBhE8mLc3XOXQ1JSsA==\"}";

void testBase64() {
  CHECK(codec::base64Encode(reinterpret_cast<const uint8_t*>("foobar"), 6) ==
        "Zm9vYmFy");
  CHECK(codec::base64Encode(reinterpret_cast<const uint8_t*>("f"), 1) == "Zg==");
  CHECK(codec::base64Encode(reinterpret_cast<const uint8_t*>("fo"), 2) == "Zm8=");
  CHECK(codec::base64Encode(nullptr, 0).empty());

  std::string decoded;
  CHECK(codec::base64Decode("Zm9vYmFy", decoded) && decoded == "foobar");
  CHECK(codec::base64Decode("Zg==", decoded) && decoded == "f");
  CHECK(!codec::base64Decode("abc", decoded));
  CHECK(!codec::base64Decode("AA==AAAA", decoded));
  CHECK(!codec::base64Decode("!!!!", decoded));
}

void testCrc32() {
  CHECK(codec::crc32Hex(reinterpret_cast<const uint8_t*>("123456789"), 9) ==
        "cbf43926");
  CHECK(codec::crc32Hex(nullptr, 0) == "00000000");
}

void testKnownAnswerVector() {
  std::string envelope;
  std::string error;
  CHECK(codec::encodeEnvelopeWithIv(testKey(), kFixedIv, kPlaintext, envelope,
                                    error));
  CHECK(error.empty());
  CHECK(envelope == kExpectedEnvelope);

  std::string plaintext;
  CHECK(codec::decodeEnvelope(testKey(), kExpectedEnvelope, plaintext, error));
  CHECK(error.empty());
  CHECK(plaintext == kPlaintext);
}

void testRandomIvRoundTrip() {
  for (size_t length = 1; length <= 64; ++length) {
    std::string plaintext(length, 'a');
    std::string envelope;
    std::string error;
    CHECK(codec::encodeEnvelope(testKey(), plaintext, envelope, error));
    CHECK(error.empty());

    std::string decoded;
    CHECK(codec::decodeEnvelope(testKey(), envelope, decoded, error));
    CHECK(error.empty());
    CHECK(decoded == plaintext);
  }
}

void testFreshIv() {
  std::string first;
  std::string second;
  std::string error;
  CHECK(codec::encodeEnvelope(testKey(), kPlaintext, first, error));
  CHECK(codec::encodeEnvelope(testKey(), kPlaintext, second, error));
  CHECK(first != second);
}

void testRejectsTamperedEnvelope() {
  std::string plaintext;
  std::string error;
  const std::string tampered =
      "{\"checksum\":\"00000000\",\"ciphertext\":"
      "\"AAECAwQFBgcICQoLDA0ODx4nsBPs4QQ7HmmrQGXg32jp7hAwdyrMbsO+VOJE5QJ6gG"
      "CyiBhE8mLc3XOXQ1JSsA==\"}";
  CHECK(!codec::decodeEnvelope(testKey(), tampered, plaintext, error));
  CHECK(error == "checksum mismatch");

  CHECK(!codec::decodeEnvelope(wrongKey(), kExpectedEnvelope, plaintext, error));
  CHECK(!codec::decodeEnvelope(testKey(), "{}", plaintext, error));

  CHECK(!codec::decodeEnvelope(
      testKey(), "{\"checksum\":\"00000000\",\"ciphertext\":\"!!!!\"}",
      plaintext, error));
  CHECK(error == "invalid base64");
}

void testPaddingBoundaries() {
  const size_t lengths[] = {15, 16, 17, 31, 32, 33};
  for (size_t length : lengths) {
    std::string plaintext(length, 'x');
    std::string envelope;
    std::string error;
    CHECK(codec::encodeEnvelope(testKey(), plaintext, envelope, error));
    std::string decoded;
    CHECK(codec::decodeEnvelope(testKey(), envelope, decoded, error));
    CHECK(decoded == plaintext);
  }
}

void testRealKeyFromEnvironment() {
  const char* keyText = std::getenv("CODEC_KEY");
  if (keyText == nullptr) {
    std::printf("SKIP: CODEC_KEY is not set, real-key test not run\n");
    return;
  }
  if (std::strlen(keyText) != 32) {
    std::printf("FAIL: CODEC_KEY must be exactly 32 ASCII bytes\n");
    ++failures;
    return;
  }

  const auto* key = reinterpret_cast<const uint8_t*>(keyText);
  std::string envelope;
  std::string error;
  CHECK(codec::encodeEnvelope(key, kPlaintext, envelope, error));
  CHECK(error.empty());

  std::string decoded;
  CHECK(codec::decodeEnvelope(key, envelope, decoded, error));
  CHECK(error.empty());
  CHECK(decoded == kPlaintext);
  std::printf("PASS: real Adapter key round-trip\n");
}

}  // namespace

int main() {
  testBase64();
  testCrc32();
  testKnownAnswerVector();
  testRandomIvRoundTrip();
  testFreshIv();
  testRejectsTamperedEnvelope();
  testPaddingBoundaries();
  testRealKeyFromEnvironment();

  if (failures == 0) {
    std::printf("PASS: all codec tests passed\n");
    return 0;
  }

  std::printf("FAIL: %d check(s) failed\n", failures);
  return 1;
}
