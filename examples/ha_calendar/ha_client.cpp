#include "ha_client.h"

#include "config.h"
#include "Arduino.h"
#include "secrets.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

static const char *HA_HOST = HA_HOST_ADDR;
static const uint16_t HA_PORT = HA_PORT_NUM;

extern unsigned long wifiLingerUntil;

// Simple connectivity helpers used across modules
static void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED)
    return;

  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  uint8_t tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 40) {
    delay(500);
    Serial.print(".");
    tries++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi connected, IP: ");
    Serial.println(WiFi.localIP());
    wifiLingerUntil = millis() + 20000; // keep WiFi up for 20s after connect
  } else {
    Serial.println("WiFi connect failed");
  }
}

bool ensureWiFi() {
  if (WiFi.status() == WL_CONNECTED)
    return true;
  connectWiFi();
  return WiFi.status() == WL_CONNECTED;
}

String buildHaUrl(const String &path) {
  String scheme = HA_USE_HTTPS ? "https" : "http";
  String url = scheme + "://" + HA_HOST + ":" + HA_PORT + path;
  return url;
}

bool fetchJson(const String &url, JsonDocument &doc) {
  Serial.print("fetchJson: Connecting to WiFi...");
  if (!ensureWiFi()) {
    Serial.println(" FAILED - WiFi not connected");
    return false;
  }
  Serial.println(" OK");

  Serial.print("fetchJson: Starting HTTP request to: ");
  Serial.println(url);

  HTTPClient http;
  WiFiClient plainClient;
  WiFiClientSecure secureClient;
  http.setTimeout(7000); // tighter timeout
  int attempts = 0;
  while (attempts < 2) {
    bool beginOk = false;
    if (HA_USE_HTTPS) {
      secureClient.setInsecure(); // Allow self-signed HA certs; set your CA for stricter security
      beginOk = http.begin(secureClient, url);
    } else {
      beginOk = http.begin(plainClient, url);
    }
    if (!beginOk) {
      Serial.println("ERROR: http.begin() failed (invalid URL or client setup)");
      return false;
    }
    http.addHeader("Authorization", String("Bearer ") + HA_TOKEN_VALUE);
    http.addHeader("Content-Type", "application/json");

    int httpCode = http.GET();
    Serial.print("fetchJson: HTTP response code: ");
    Serial.println(httpCode);

    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      http.end();

      if (payload.length() == 0) {
        Serial.println("ERROR: Empty response payload");
        return false;
      }

      Serial.print("fetchJson: Payload length: ");
      Serial.println(payload.length());
      if (payload.length() < 200) {
        Serial.print("fetchJson: Payload preview: ");
        Serial.println(payload);
      } else {
        Serial.print("fetchJson: Payload preview (first 200 chars): ");
        Serial.println(payload.substring(0, 200));
      }

      DeserializationError err = deserializeJson(doc, payload);
      if (err) {
        Serial.print("ERROR: JSON parsing failed: ");
        Serial.println(err.c_str());
        Serial.print("JSON error code: ");
        Serial.println(err.code());
        Serial.print("Payload start: ");
        Serial.println(payload.substring(0, 100));
        return false;
      }

      Serial.println("fetchJson: Success");
      return true;
    }

    String errorPayload = http.getString();
    Serial.printf("ERROR: HTTP Error: %d\n", httpCode);
    Serial.print("Error response: ");
    Serial.println(errorPayload.length() ? errorPayload : "(empty response)");
    http.end();
    attempts++;
    if (attempts < 2) {
      Serial.println("Retrying fetchJson...");
      delay(250);
    }
  }
  return false;
}

bool fetchJsonPost(const String &url, const String &payload, JsonDocument &doc) {
  Serial.print("fetchJsonPost: Connecting to WiFi...");
  if (!ensureWiFi()) {
    Serial.println("FAILED - WiFi not connected");
    return false;
  }
  Serial.println("OK");

  Serial.print("fetchJsonPost: URL: ");
  Serial.println(url);
  Serial.print("fetchJsonPost: Payload: ");
  Serial.println(payload);

  HTTPClient http;
  WiFiClient plainClient;
  WiFiClientSecure secureClient;
  bool beginOk = false;
  if (HA_USE_HTTPS) {
    secureClient.setInsecure();
    beginOk = http.begin(secureClient, url);
  } else {
    beginOk = http.begin(plainClient, url);
  }
  if (!beginOk) {
    Serial.println("ERROR: http.begin() failed (invalid URL or client setup)");
    return false;
  }
  http.setTimeout(7000); // tighter timeout
  http.addHeader("Authorization", String("Bearer ") + HA_TOKEN_VALUE);
  http.addHeader("Content-Type", "application/json");

  int httpCode = http.POST(payload);
  Serial.print("fetchJsonPost: HTTP response code: ");
  Serial.println(httpCode);

  String response =
      http.getString(); // Get response regardless of code for debug

  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("ERROR: HTTP POST failed with code: %d\n", httpCode);
    Serial.print("Response: ");
    if (response.length() > 0) {
      Serial.println(response);
    } else {
      Serial.println("(empty response)");
    }
    http.end();
    return false;
  }

  http.end();

  if (response.length() == 0) {
    Serial.println("ERROR: Empty response payload");
    return false;
  }

  Serial.print("fetchJsonPost: Response length: ");
  Serial.println(response.length());

  DeserializationError err = deserializeJson(doc, response);
  if (err) {
    Serial.print("ERROR: JSON parsing failed: ");
    Serial.println(err.c_str());
    Serial.print("JSON error code: ");
    Serial.println(err.code());
    Serial.print("Response start: ");
    Serial.println(response.substring(0, 100));
    return false;
  }

  Serial.println("fetchJsonPost: Success");
  return true;
}
