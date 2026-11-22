#pragma once

// Copy this file to secrets.h and fill in the local values.
// secrets.h is excluded from Git. Never commit real credentials.
//
// CODEC_KEY must be exactly 32 ASCII bytes taken from the Adapter config
// as-is. Do not hex-decode it.
#define WIFI_SSID "CHANGE_ME"
#define WIFI_PASSWORD "CHANGE_ME"
#define CODEC_KEY "CHANGE_ME_32_ASCII_BYTES"

#define DEVICE_ID 1
#define ADAPTER_URL "http://192.168.0.197:8081/devices/1/telemetry"
