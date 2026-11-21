#!/bin/sh
set -eu

cd "$(dirname "$0")/.."

CONFIG="$(cd ../.. && pwd)/build/config/AdapterConfig.json"
if [ -f "$CONFIG" ] && command -v python3 >/dev/null 2>&1; then
    CODEC_KEY=$(python3 -c \
        'import json,sys; print(json.load(open(sys.argv[1]))["custom_config"]["codec_key"])' \
        "$CONFIG")
    export CODEC_KEY
fi

mkdir -p .pio/native-tests

g++ -std=c++17 -Wall -Wextra -pedantic \
    -I src \
    src/codec.cpp test/test_codec.cpp \
    -lcrypto \
    -o .pio/native-tests/codec_tests

./.pio/native-tests/codec_tests
