#include "codec/Decoder.h"

#include "codec/Base64.h"
#include "codec/Checksum.h"

#include <openssl/evp.h>
#include <json/json.h>

#include <cstring>
#include <sstream>

namespace codec {
namespace {

std::string normalizeKey(const std::string &key) {
  std::string k = key;
  if (k.size() < 32) k.resize(32, '\0');
  else if (k.size() > 32) k.resize(32);
  return k;
}

}  // namespace

Decoder::Decoder(std::string key) : key_(normalizeKey(key)) {}

bool Decoder::decode(const std::string &envelopeJson,
                     std::string &plaintext,
                     std::string &error) const {
  plaintext.clear();
  error.clear();

  Json::Value env;
  Json::CharReaderBuilder rb;
  std::istringstream iss(envelopeJson);
  std::string parseErr;
  if (!Json::parseFromStream(rb, iss, &env, &parseErr)) {
    error = "Decoder: invalid envelope JSON: " + parseErr;
    return false;
  }
  if (!env.isObject() || !env["checksum"].isString() ||
      !env["ciphertext"].isString()) {
    error = "Decoder: envelope must contain 'checksum' and 'ciphertext' strings";
    return false;
  }

  std::string blob;
  if (!base64Decode(env["ciphertext"].asString(), blob)) {
    error = "Decoder: invalid base64 in 'ciphertext'";
    return false;
  }

  const std::string expected = env["checksum"].asString();
  const std::string actual   = crc32Hex(blob);
  if (expected != actual) {
    error = "Decoder: checksum mismatch (expected " + expected +
            ", got " + actual + ")";
    return false;
  }

  if (blob.size() < 16) {
    error = "Decoder: ciphertext too short (missing IV)";
    return false;
  }

  unsigned char iv[16];
  std::memcpy(iv, blob.data(), 16);
  const std::string cipher = blob.substr(16);

  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  if (!ctx) {
    error = "Decoder: EVP_CIPHER_CTX_new failed";
    return false;
  }

  plaintext.resize(cipher.size() + 16);
  int outLen = 0;
  int finalLen = 0;
  const bool ok =
      EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr,
                         reinterpret_cast<const unsigned char *>(key_.data()),
                         iv) == 1 &&
      EVP_DecryptUpdate(ctx, reinterpret_cast<unsigned char *>(plaintext.data()),
                        &outLen,
                        reinterpret_cast<const unsigned char *>(cipher.data()),
                        static_cast<int>(cipher.size())) == 1 &&
      EVP_DecryptFinal_ex(ctx,
                          reinterpret_cast<unsigned char *>(plaintext.data()) + outLen,
                          &finalLen) == 1;
  EVP_CIPHER_CTX_free(ctx);

  if (!ok) {
    plaintext.clear();
    error = "Decoder: AES decryption failed (bad key or corrupted payload)";
    return false;
  }

  plaintext.resize(static_cast<std::size_t>(outLen) + finalLen);
  return true;
}

}  // namespace codec