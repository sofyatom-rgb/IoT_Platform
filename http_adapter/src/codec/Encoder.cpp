#include "codec/Encoder.h"

#include "codec/Base64.h"
#include "codec/Checksum.h"

#include <openssl/evp.h>
#include <openssl/rand.h>

#include <json/json.h>

#include <stdexcept>

namespace codec {
namespace {

std::string normalizeKey(const std::string &key) {
  std::string k = key;
  if (k.size() < 32) k.resize(32, '\0');
  else if (k.size() > 32) k.resize(32);
  return k;
}

}  // namespace

Encoder::Encoder(std::string key) : key_(normalizeKey(key)) {}

std::string Encoder::encode(const std::string &plaintext) const {
  unsigned char iv[16];
  if (RAND_bytes(iv, sizeof(iv)) != 1) {
    throw std::runtime_error("Encoder: RAND_bytes failed");
  }

  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  if (!ctx) throw std::runtime_error("Encoder: EVP_CIPHER_CTX_new failed");

  std::string cipher(plaintext.size() + 16, '\0');
  int outLen = 0;
  int finalLen = 0;

  const bool ok =
      EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr,
                         reinterpret_cast<const unsigned char *>(key_.data()),
                         iv) == 1 &&
      EVP_EncryptUpdate(ctx, reinterpret_cast<unsigned char *>(cipher.data()),
                        &outLen,
                        reinterpret_cast<const unsigned char *>(plaintext.data()),
                        static_cast<int>(plaintext.size())) == 1 &&
      EVP_EncryptFinal_ex(ctx,
                          reinterpret_cast<unsigned char *>(cipher.data()) + outLen,
                          &finalLen) == 1;
  EVP_CIPHER_CTX_free(ctx);
  if (!ok) throw std::runtime_error("Encoder: AES encryption failed");

  cipher.resize(static_cast<std::size_t>(outLen) + finalLen);

  // IV || ciphertext
  std::string blob(reinterpret_cast<char *>(iv), sizeof(iv));
  blob += cipher;

  Json::Value env;
  env["checksum"]   = crc32Hex(blob);
  env["ciphertext"] = base64Encode(blob);

  Json::StreamWriterBuilder wb;
  wb["indentation"] = "";
  wb["emitUTF8"]    = true;
  return Json::writeString(wb, env);
}

}  // namespace codec