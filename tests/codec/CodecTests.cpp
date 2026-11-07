#include "TestFramework.h"

#include "codec/Base64.h"
#include "codec/Checksum.h"
#include "codec/Decoder.h"
#include "codec/Encoder.h"

#include <json/json.h>

#include <sstream>
#include <string>
#include <vector>

namespace {
std::string makeEnvelope(const std::string &blob, const std::string &checksum) {
  Json::Value envelope;
  envelope["checksum"] = checksum;
  envelope["ciphertext"] = codec::base64Encode(blob);
  Json::StreamWriterBuilder builder;
  builder["indentation"] = "";
  return Json::writeString(builder, envelope);
}
}  // namespace

TEST_CASE(encoder_and_decoder_round_trip_text_utf8_binary_and_empty_payloads) {
  codec::Encoder encoder("unit-test-key");
  codec::Decoder decoder("unit-test-key");
  const std::vector<std::string> payloads = {
      "", "hello", "Привет, IoT!", std::string("binary\0payload", 14),
      std::string(2048, 'x')};
  for (const auto &plain : payloads) {
    std::string decoded;
    std::string error;
    EXPECT_TRUE(decoder.decode(encoder.encode(plain), decoded, error));
    EXPECT_EQ(plain, decoded);
    EXPECT_EQ("", error);
  }
}

TEST_CASE(encoder_uses_a_fresh_initialization_vector) {
  codec::Encoder encoder("same-key");
  EXPECT_TRUE(encoder.encode("same plaintext") != encoder.encode("same plaintext"));
}

TEST_CASE(decoder_rejects_malformed_envelopes_and_clears_stale_output) {
  codec::Decoder decoder("correct-key");
  std::string plain = "old value";
  std::string error;

  EXPECT_FALSE(decoder.decode("not-json", plain, error));
  EXPECT_EQ("", plain);
  EXPECT_TRUE(error.find("invalid envelope JSON") != std::string::npos);

  EXPECT_FALSE(decoder.decode("{}", plain, error));
  EXPECT_TRUE(error.find("checksum") != std::string::npos ||
              error.find("ciphertext") != std::string::npos);

  EXPECT_FALSE(decoder.decode(
      R"({"checksum":"00000000","ciphertext":"!!!!"})", plain, error));
  EXPECT_TRUE(error.find("invalid base64") != std::string::npos);
}

TEST_CASE(decoder_checks_checksum_iv_and_decryption_key) {
  codec::Encoder encoder("right-key");
  codec::Decoder wrongKey("wrong-key");
  std::string plain;
  std::string error;
  EXPECT_FALSE(wrongKey.decode(encoder.encode("secret"), plain, error));
  EXPECT_EQ("", plain);
  EXPECT_TRUE(error.find("decryption failed") != std::string::npos);

  const std::string tamperedBlob(32, '\x01');
  EXPECT_FALSE(wrongKey.decode(makeEnvelope(tamperedBlob, "00000000"), plain, error));
  EXPECT_TRUE(error.find("checksum mismatch") != std::string::npos);

  EXPECT_FALSE(wrongKey.decode(makeEnvelope("", codec::crc32Hex("")), plain, error));
  EXPECT_TRUE(error.find("too short") != std::string::npos);
}
