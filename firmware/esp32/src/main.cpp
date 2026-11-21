#include <Arduino.h>
#include <DallasTemperature.h>
#include <HTTPClient.h>
#include <OneWire.h>
#include <WiFi.h>

#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "secrets.h"

static_assert(DEVICE_ID > 0, "DEVICE_ID must be a positive integer");

namespace {

constexpr uint8_t kOneWirePin = 4;
constexpr uint32_t kWifiTimeoutMs = 15000;
constexpr uint32_t kHttpConnectTimeoutMs = 5000;
constexpr uint32_t kHttpTimeoutMs = 10000;
constexpr uint32_t kMeasurementIntervalMs = 60000;
constexpr uint8_t kSensorAttempts = 3;

OneWire oneWire(kOneWirePin);
DallasTemperature sensors(&oneWire);

bool readTemperature(float& temperature) {
  for (uint8_t attempt = 1; attempt <= kSensorAttempts; ++attempt) {
    sensors.requestTemperatures();
    const float value = sensors.getTempCByIndex(0);

    if (value == DEVICE_DISCONNECTED_C) {
      Serial.printf("[sensor] attempt %u: sensor disconnected\n",
                    static_cast<unsigned>(attempt));
    } else if (value == 85.0f) {
      Serial.printf("[sensor] attempt %u: power-on value 85.0 C\n",
                    static_cast<unsigned>(attempt));
    } else if (std::isnan(value) || value < -55.0f || value > 125.0f) {
      Serial.printf("[sensor] attempt %u: value out of range\n",
                    static_cast<unsigned>(attempt));
    } else {
      temperature = value;
      return true;
    }

    delay(250);
  }
  return false;
}

bool connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  const uint32_t started = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - started < kWifiTimeoutMs) {
    delay(250);
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[wifi] connection timeout");
    return false;
  }

  Serial.println("[wifi] connected");
  // Адрес нужен, чтобы по серийному логу было видно, в какой сети оказалась плата.
  Serial.print("[wifi] ip: ");
  Serial.println(WiFi.localIP());
  Serial.print("[wifi] gateway: ");
  Serial.println(WiFi.gatewayIP());
  Serial.print("[wifi] mask: ");
  Serial.println(WiFi.subnetMask());
  return true;
}

bool sendTelemetry(const std::string& payload) {
  WiFiClient client;
  HTTPClient http;
  http.setConnectTimeout(kHttpConnectTimeoutMs);
  http.setTimeout(kHttpTimeoutMs);

  if (!http.begin(client, ADAPTER_URL)) {
    Serial.println("[http] begin failed");
    return false;
  }

  http.addHeader("Content-Type", "application/json");

  std::vector<uint8_t> body(payload.begin(), payload.end());
  const int status = http.POST(body.data(), body.size());

  Serial.printf("[http] status=%d\n", status);
  if (status < 0) {
    Serial.printf("[http] error: %s\n", http.errorToString(status).c_str());
  } else if (status != 201 && status != 400 && status != 502) {
    Serial.printf("[http] unexpected status: %d\n", status);
  }
  http.end();
  return status == 201 || status == 502;
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);

  Serial.println();
  Serial.println("[boot] ESP32 DS18B20 telemetry client (periodic mode)");

  sensors.begin();
  sensors.setResolution(12);
  const uint8_t deviceCount = sensors.getDeviceCount();
  Serial.printf("[sensor] devices found: %u\n",
                static_cast<unsigned>(deviceCount));
  if (deviceCount == 0) {
    Serial.println("[sensor] no DS18B20 found, stop");
    return;
  }

  // Wi-Fi connect once at startup
  if (!connectWifi()) {
    Serial.println("[wifi] failed to connect at startup, retry in loop");
  }
}

void loop() {
  static uint32_t lastMeasurement = 0;
  static bool firstCycle = true;
  static std::string pendingPayload;
  static uint8_t failedSends = 0;
  const uint32_t now = millis();

  // Первое измерение делаем сразу после подключения к Wi-Fi
  if (!firstCycle && now - lastMeasurement < kMeasurementIntervalMs) {
    return;
  }
  firstCycle = false;
  lastMeasurement = now;

  // Неудачную отправку повторяем в следующем цикле, пока сеть не восстановится
  if (!pendingPayload.empty()) {
    if (WiFi.status() != WL_CONNECTED) {
      connectWifi();
    }
    Serial.println("[http] retry previous reading");
    if (sendTelemetry(pendingPayload)) {
      pendingPayload.clear();
      failedSends = 0;
    } else if (++failedSends >= 2) {
      // Wi-Fi может оставаться "подключённым" без маршрута к адаптеру
      Serial.println("[wifi] reconnecting after send failures");
      WiFi.disconnect();
      connectWifi();
      failedSends = 0;
      return;
    } else {
      return;
    }
  }

  float temperature = 0.0f;
  if (!readTemperature(temperature)) {
    Serial.println("[sensor] no valid temperature this cycle");
    // Still report Wi-Fi status so user can see connectivity
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[wifi] disconnected");
    }
    return;
  }
  Serial.printf("[sensor] temperature: %.2f C\n", temperature);

  // REST: device_id уже в URL, в теле только temperature.
  char payload[64];
  std::snprintf(payload, sizeof(payload), "{\"temperature\":%.4f}",
                static_cast<double>(temperature));
  Serial.printf("[payload] %s\n", payload);

  // Send if Wi-Fi connected, otherwise try to connect
  if (WiFi.status() != WL_CONNECTED) {
    if (!connectWifi()) {
      Serial.println("[wifi] not connected, skip send this cycle");
      return;
    }
  }

  if (!sendTelemetry(payload)) {
    pendingPayload = payload;
    return;
  }

  // Wi-Fi stays connected; next cycle will use millis() timing
}