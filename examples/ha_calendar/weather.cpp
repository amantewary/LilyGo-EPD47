#include "weather.h"

#include "config.h"
#include "ha_client.h"
#include "types.h"
#include <ArduinoJson.h>

extern JsonDocument haDoc;
void disableWiFiIfAllowed();

void fetchWeather(WeatherData &currentWeather) {
  Serial.println("=== fetchWeather() called ===");
  String url = buildHaUrl(String("/api/states/") + String(ENTITY_WEATHER));
  Serial.print("Weather URL: ");
  Serial.println(url);

  haDoc.clear();

  if (!fetchJson(url, haDoc)) {
    Serial.println("ERROR: Weather fetch failed - fetchJson returned false");
    return;
  }

  Serial.println("Weather JSON fetched successfully");

  const char *state = haDoc["state"];

  // Check if temperature exists and is valid
  if (!haDoc["attributes"]["temperature"].is<float>()) {
    Serial.println("WARNING: Temperature not found or invalid in JSON");
    currentWeather.temperature = "-- C";
  } else {
    float temp = haDoc["attributes"]["temperature"];
    float displayTemp = temp;
    const char *unit = " C";
    if (USE_FAHRENHEIT) {
      displayTemp = temp * 9.0 / 5.0 + 32.0;
      unit = " F";
    }
    currentWeather.temperature = String(displayTemp, 1) + unit;
  }

  currentWeather.condition = state ? String(state) : "--";

  Serial.print("Weather condition: ");
  Serial.println(currentWeather.condition);
  Serial.print("Weather temperature: ");
  Serial.println(currentWeather.temperature);
  Serial.println("=== fetchWeather() complete ===");
  disableWiFiIfAllowed();
}
