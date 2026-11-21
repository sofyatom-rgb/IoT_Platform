#include "codec.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <vector>

#if defined(ESP32)
#include <mbedtls/aes.h>
#if __has_include(<esp_random.h>)
#include <esp_random.h>
#else
#include <esp_system.h>
#endif
#else
#include <openssl/evp.h>
#include <openssl/rand.h>
#endif

namespace codec {
namespace {

const char kBase64Alphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int base64Value(char c) {
  const char* p = std::strchr(kBase64Alphabet, c);
  if (p == nullptr) return -1;
  return static_cast<int>(p - kBase64Alphabet);
}

bool randomBytes(uint8_t* out, size_t len) {
#if defined(ESP32)
  for (size_t i = 0; i < len; i += 4) {
    const uint32_t value = esp_random();
    for (size_t j = 0; j < 4 && i + j < len; ++j) {
      out[i + j] = static_cast<uint8_t>(value >> (8 * j));
    }
  }
  return true;
#else
  return RAND_bytes(out, static_cast<int>(len)) == 1;
#endif
}

bool aes256CbcEncrypt(const uint8_t* key, const uint8_t* iv,
                      const uint8_t* data, size_t len,
                      std::vector<uint8_t>& out) {
  const size_t padding = 16 - (len % 16);
  std::vector<uint8_t> padded;
  if (len > 0) padded.assign(data, data + len);
  padded.insert(padded.end(), padding, static_cast<uint8_t>(padding));

#if defined(ESP32)
  uint8_t ivCopy[16];
  std::memcpy(ivCopy, iv, sizeof(ivCopy));
  out.assign(padded.size(), 0);

  mbedtls_aes_context ctx;
  mbedtls_aes_init(&ctx);
  int rc = mbedtls_aes_setkey_enc(&ctx, key, 256);
  if (rc == 0) {
    rc = mbedtls_aes_crypt_cbc(&ctx, MBEDTLS_AES_ENCRYPT, padded.size(),
                               ivCopy, padded.data(), out.data());
  }
  mbedtls_aes_free(&ctx);
  return rc == 0;
#else
  out.assign(padded.size() + 16, 0);
  EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
  if (ctx == nullptr) return false;

  int outLen = 0;
  int finalLen = 0;
  const bool ok =
      EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key, iv) == 1 &&
      EVP_CIPHER_CTX_set_padding(ctx, 0) == 1 &&
      EVP_EncryptUpdate(ctx, out.data(), &outLen, padded.data(),
                        static_cast<int>(padded.size())) == 1 &&
      EVP_EncryptFinal_ex(ctx, out.data() + outLen, &finalLen) == 1;
  EVP_CIPHER_CTX_free(ctx);
  if (!ok) return false;

  out.resize(static_cast<size_t>(outLen + finalLen));
  return true;
#endif
}

bool aes256CbcDecrypt(const uint8_t* key, const uint8_t* iv,
                      const uint8_t* data, size_t len,
                      std::vector<uint8_t>& out) {
  if (len == 0 || len % 16 != 0) return false;

#if defined(ESP32)
  uint8_t ivCopy[16];
  std::memcpy(ivCopy, iv, sizeof(ivCopy));
  out.assign(len, 0);

  mbedtls_aes_context ctx;
  mbedtls_aes_init(&ctx);
  int rc = mbedtls_aes_setkey_dec(&ctx, key, 256);
  if (rc == 0) {
    rc = mbedtls_aes_crypt_cbc(&ctx, MBEDTLS_AES_DECRYPT, len, ivCopy,
                               data, out.data());
  }
  mbedtls_aes_free(&ctx);
  if (rc != 0) return false;
#else
  out.assign(len + 16, 0);
  EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
  if (ctx == nullptr) return false;

  int outLen = 0;
  int finalLen = 0;
  const bool ok =
      EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key, iv) == 1 &&
      EVP_CIPHER_CTX_set_padding(ctx, 0) == 1 &&
      EVP_DecryptUpdate(ctx, out.data(), &outLen, data,
                        static_cast<int>(len)) == 1 &&
      EVP_DecryptFinal_ex(ctx, out.data() + outLen, &finalLen) == 1;
  EVP_CIPHER_CTX_free(ctx);
  if (!ok) return false;

  out.resize(static_cast<size_t>(outLen + finalLen));
#endif

  const uint8_t padding = out.back();
  if (padding == 0 || padding > 16 || padding > out.size()) return false;
  for (size_t i = out.size() - padding; i < out.size(); ++i) {
    if (out[i] != padding) return false;
  }
  out.resize(out.size() - padding);
  return true;
}

bool buildEnvelope(const uint8_t* key, const uint8_t* iv,
                   const std::string& plaintext, std::string& envelopeJson,
                   std::string& error) {
  std::vector<uint8_t> ciphertext;
  if (!aes256CbcEncrypt(key, iv,
                        reinterpret_cast<const uint8_t*>(plaintext.data()),
                        plaintext.size(), ciphertext)) {
    error = "AES encryption failed";
    return false;
  }

  std::vector<uint8_t> blob(iv, iv + 16);
  blob.insert(blob.end(), ciphertext.begin(), ciphertext.end());

  envelopeJson = "{\"checksum\":\"" + crc32Hex(blob.data(), blob.size()) +
                 "\",\"ciphertext\":\"" +
                 base64Encode(blob.data(), blob.size()) + "\"}";
  error.clear();
  return true;
}

bool extractStringField(const std::string& json, const char* name,
                        std::string& out) {
  out.clear();
  const std::string needle = std::string("\"") + name + "\"";
  size_t pos = json.find(needle);
  if (pos == std::string::npos) return false;

  pos += needle.size();
  while (pos < json.size() &&
         std::isspace(static_cast<unsigned char>(json[pos]))) {
    ++pos;
  }
  if (pos >= json.size() || json[pos] != ':') return false;
  ++pos;
  while (pos < json.size() &&
         std::isspace(static_cast<unsigned char>(json[pos]))) {
    ++pos;
  }
  if (pos >= json.size() || json[pos] != '"') return false;
  ++pos;

  std::string value;
  while (pos < json.size()) {
    const char c = json[pos++];
    if (c == '"') {
      out = value;
      return true;
    }
    if (c == '\\' || static_cast<unsigned char>(c) < 0x20) return false;
    value.push_back(c);
  }
  return false;
}

}  // namespace

std::string crc32Hex(const uint8_t* data, size_t len) {
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; ++bit) {
      crc = (crc & 1u) ? (0xEDB88320u ^ (crc >> 1)) : (crc >> 1);
    }
  }
  crc ^= 0xFFFFFFFFu;

  char text[9];
  std::snprintf(text, sizeof(text), "%08lx",
                static_cast<unsigned long>(crc));
  return std::string(text);
}

std::string base64Encode(const uint8_t* data, size_t len) {
  std::string out;
  out.reserve(((len + 2) / 3) * 4);

  size_t i = 0;
  while (i + 3 <= len) {
    const uint32_t value = (static_cast<uint32_t>(data[i]) << 16) |
                           (static_cast<uint32_t>(data[i + 1]) << 8) |
                           static_cast<uint32_t>(data[i + 2]);
    out.push_back(kBase64Alphabet[(value >> 18) & 0x3F]);
    out.push_back(kBase64Alphabet[(value >> 12) & 0x3F]);
    out.push_back(kBase64Alphabet[(value >> 6) & 0x3F]);
    out.push_back(kBase64Alphabet[value & 0x3F]);
    i += 3;
  }

  const size_t remaining = len - i;
  if (remaining == 1) {
    const uint32_t value = static_cast<uint32_t>(data[i]) << 16;
    out.push_back(kBase64Alphabet[(value >> 18) & 0x3F]);
    out.push_back(kBase64Alphabet[(value >> 12) & 0x3F]);
    out.push_back('=');
    out.push_back('=');
  } else if (remaining == 2) {
    const uint32_t value =
        (static_cast<uint32_t>(data[i]) << 16) |
        (static_cast<uint32_t>(data[i + 1]) << 8);
    out.push_back(kBase64Alphabet[(value >> 18) & 0x3F]);
    out.push_back(kBase64Alphabet[(value >> 12) & 0x3F]);
    out.push_back(kBase64Alphabet[(value >> 6) & 0x3F]);
    out.push_back('=');
  }
  return out;
}

bool base64Decode(const std::string& text, std::string& out) {
  out.clear();
  if (text.size() % 4 != 0) return false;

  std::string decoded;
  decoded.reserve((text.size() / 4) * 3);

  for (size_t i = 0; i < text.size(); i += 4) {
    const bool lastBlock = (i + 4 == text.size());
    const char c0 = text[i];
    const char c1 = text[i + 1];
    const char c2 = text[i + 2];
    const char c3 = text[i + 3];

    if (c0 == '=' || c1 == '=') return false;

    const int v0 = base64Value(c0);
    const int v1 = base64Value(c1);
    if (v0 < 0 || v1 < 0) return false;

    const bool pad2 = (c2 == '=');
    const bool pad1 = (c3 == '=');
    if (pad2 && !pad1) return false;
    if ((pad1 || pad2) && !lastBlock) return false;

    const int v2 = pad2 ? 0 : base64Value(c2);
    const int v3 = pad1 ? 0 : base64Value(c3);
    if ((!pad2 && v2 < 0) || (!pad1 && v3 < 0)) return false;

    if (pad2 && (v1 & 0x0F) != 0) return false;
    if (!pad2 && pad1 && (v2 & 0x03) != 0) return false;

    const uint32_t value = (static_cast<uint32_t>(v0) << 18) |
                           (static_cast<uint32_t>(v1) << 12) |
                           (static_cast<uint32_t>(v2) << 6) |
                           static_cast<uint32_t>(v3);

    decoded.push_back(static_cast<char>((value >> 16) & 0xFF));
    if (!pad2) decoded.push_back(static_cast<char>((value >> 8) & 0xFF));
    if (!pad1) decoded.push_back(static_cast<char>(value & 0xFF));
  }

  out = decoded;
  return true;
}

bool encodeEnvelope(const uint8_t key[32], const std::string& plaintext,
                    std::string& envelopeJson, std::string& error) {
  uint8_t iv[16];
  if (!randomBytes(iv, sizeof(iv))) {
    error = "random IV generation failed";
    return false;
  }
  return buildEnvelope(key, iv, plaintext, envelopeJson, error);
}

bool encodeEnvelopeWithIv(const uint8_t key[32], const uint8_t iv[16],
                          const std::string& plaintext,
                          std::string& envelopeJson, std::string& error) {
  return buildEnvelope(key, iv, plaintext, envelopeJson, error);
}

bool decodeEnvelope(const uint8_t key[32], const std::string& envelopeJson,
                    std::string& plaintext, std::string& error) {
  plaintext.clear();

  std::string checksum;
  std::string ciphertext;
  if (!extractStringField(envelopeJson, "checksum", checksum) ||
      !extractStringField(envelopeJson, "ciphertext", ciphertext)) {
    error = "invalid envelope JSON";
    return false;
  }

  std::string blob;
  if (!base64Decode(ciphertext, blob)) {
    error = "invalid base64";
    return false;
  }
  if (blob.size() < 16) {
    error = "blob too short";
    return false;
  }

  const uint8_t* blobData = reinterpret_cast<const uint8_t*>(blob.data());
  if (crc32Hex(blobData, blob.size()) != checksum) {
    error = "checksum mismatch";
    return false;
  }

  const uint8_t* iv = blobData;
  const uint8_t* encrypted = blobData + 16;
  const size_t encryptedLen = blob.size() - 16;

  std::vector<uint8_t> decrypted;
  if (!aes256CbcDecrypt(key, iv, encrypted, encryptedLen, decrypted)) {
    error = "decryption failed";
    return false;
  }

  if (!decrypted.empty()) {
    plaintext.assign(reinterpret_cast<const char*>(decrypted.data()),
                     decrypted.size());
  }
  error.clear();
  return true;
}

}  // namespace codec
