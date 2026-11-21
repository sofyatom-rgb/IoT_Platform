#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace codec {

// CRC32 IEEE 802.3, 8 lowercase hex characters.
std::string crc32Hex(const uint8_t* data, size_t len);

// RFC 4648 Base64 with '=' padding.
std::string base64Encode(const uint8_t* data, size_t len);
bool base64Decode(const std::string& text, std::string& out);

// Builds {"checksum":"...","ciphertext":"..."} with a random 16-byte IV.
bool encodeEnvelope(const uint8_t key[32], const std::string& plaintext,
                    std::string& envelopeJson, std::string& error);

// Same as encodeEnvelope, but with a caller-provided IV.
// Used only by compatibility tests; production code must use encodeEnvelope.
bool encodeEnvelopeWithIv(const uint8_t key[32], const uint8_t iv[16],
                          const std::string& plaintext,
                          std::string& envelopeJson, std::string& error);

bool decodeEnvelope(const uint8_t key[32], const std::string& envelopeJson,
                    std::string& plaintext, std::string& error);

}  // namespace codec
