#pragma once

#include <string>

namespace codec {

// Классический Base64 (RFC 4648, с паддингом '=').
std::string base64Encode(const std::string &data);
bool        base64Decode(const std::string &text, std::string &out);

}  // namespace codec