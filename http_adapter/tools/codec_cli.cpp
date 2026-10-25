#include <iostream>
#include <string>

#include "codec/Decoder.h"
#include "codec/Encoder.h"

int main(int argc, char *argv[]) {
  if (argc < 4) {
    std::cerr << "Usage:\n"
              << "  " << argv[0] << " encode <key> <plaintext|->\n"
              << "  " << argv[0] << " decode <key> <envelope-json|->\n";
    return 1;
  }

  const std::string mode  = argv[1];
  const std::string key   = argv[2];
  const std::string input = argv[3];

  std::string payload = input;
  if (input == "-") {                       // читать со stdin
    payload.clear();
    std::string line;
    while (std::getline(std::cin, line)) payload += line;
  }

  if (mode == "encode") {
    codec::Encoder enc(key);
    std::cout << enc.encode(payload) << std::endl;
    return 0;
  }
  if (mode == "decode") {
    codec::Decoder dec(key);
    std::string plain, err;
    if (!dec.decode(payload, plain, err)) {
      std::cerr << "Error: " << err << std::endl;
      return 2;
    }
    std::cout << plain << std::endl;
    return 0;
  }

  std::cerr << "Unknown mode: " << mode << std::endl;
  return 1;
}