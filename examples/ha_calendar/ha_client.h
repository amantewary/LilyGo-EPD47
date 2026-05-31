#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

String buildHaUrl(const String &path);
bool ensureWiFi();
bool fetchJson(const String &url, JsonDocument &doc);
bool fetchJsonPost(const String &url, const String &payload, JsonDocument &doc);
