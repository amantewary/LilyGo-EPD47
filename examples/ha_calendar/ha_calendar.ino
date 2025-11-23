/**
 * Home Assistant Dashboard for LilyGo EPD47
 *
 * A comprehensive e-paper dashboard that displays:
 * - Date (top-left)
 * - Weather information with icons
 * - Daily motivational quotes (rotated every 6 hours)
 * - Todo list items (due today)
 * - Upcoming calendar events (next 7 days)
 * - Mini calendar week view
 * - WiFi connection status
 * - Battery voltage logged (not drawn)
 *
 * Features:
 * - Partial refresh for fast updates and minimal flashing
 * - OTA (Over-The-Air) updates via WiFi; WiFi disabled between fetches unless OTA window is active
 * - Optimized refresh rates (hourly weather, 6-hour quotes, 6-hour todo/calendar, midnight full refresh)
 * - On-demand OTA window opened by BUTTON_1 (GPIO21) for 5 minutes
 *
 * Hardware: LilyGo T5-ePaper-S3 (ESP32-S3, 4.7" EPD, 960x540)
 * Framework: Arduino/PlatformIO
 */

#ifndef BOARD_HAS_PSRAM
#error "Please enable PSRAM, Arduino IDE -> Tools -> PSRAM -> OPI PSRAM"
#endif

#include "calendar_icons.h"
#include "config.h" // Entity configuration (not tracked by git)
#include "epd_driver.h"
#include "esp_adc_cal.h" // For ADC calibration
#include "esp_sleep.h"
#include "firasans.h"
#include "firasans_medium.h"
#include "firasans_small.h"
#include "secrets.h"
#include "todo_icons.h"
#include "utilities.h"
#include "weather_icons.h"
#include "wifi_icons.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <algorithm>
#include <cstring>
#include <vector>

// Shared JSON documents for Home Assistant responses
// With ArduinoJson v7, JsonDocument manages capacity dynamically.
static JsonDocument haDoc;       // Single-object responses (/api/states, service responses)
static JsonDocument haArrayDoc;  // Larger array responses (calendar, quotes)

// ---------- CONFIG ----------
// WiFi (loaded from secrets.h, which is not committed to git)
const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;

// Home Assistant (host/port loaded from secrets.h)
const char *HA_HOST = HA_HOST_ADDR;
const uint16_t HA_PORT = HA_PORT_NUM;
// Use HTTPS to talk to Home Assistant (set in config.h; defaults to HTTP)
#ifndef HA_USE_HTTPS
#define HA_USE_HTTPS 0
#endif

// Optional output formats
#ifndef USE_FAHRENHEIT
#define USE_FAHRENHEIT 0
#endif
#ifndef USE_24H_TIME
#define USE_24H_TIME 0
#endif

// Long-lived access token from HA (from secrets.h)
const char *HA_TOKEN = HA_TOKEN_VALUE;

// OTA password (keep private in secrets.h)
#ifndef OTA_PASSWORD_VALUE
#define OTA_PASSWORD_VALUE "CHANGE_ME_OTA_PASSWORD"
#endif
const char *OTA_PASSWORD = OTA_PASSWORD_VALUE;

// HA entities (loaded from config.h, which is not committed to git)
// These macros are used directly in the code - no need for const char*
// variables

// NTP Config (loaded from config.h)
const char *ntpServer = NTP_SERVER;
const long gmtOffset_sec = GMT_OFFSET_SEC;
const int daylightOffset_sec = DAYLIGHT_OFFSET_SEC;

// Build todo entities vector from config.h defines
const std::vector<const char *> ENTITY_TODOS = {
#if ENTITY_TODOS_COUNT >= 1
    ENTITY_TODO_1,
#endif
#if ENTITY_TODOS_COUNT >= 2
    ENTITY_TODO_2,
#endif
#if ENTITY_TODOS_COUNT >= 3
    ENTITY_TODO_3,
#endif
#if ENTITY_TODOS_COUNT >= 4
    ENTITY_TODO_4,
#endif
#if ENTITY_TODOS_COUNT >= 5
    ENTITY_TODO_5,
#endif
};

// Build calendar entities vector from config.h defines
const std::vector<const char *> ENTITY_CALENDARS = {
#if ENTITY_CALENDARS_COUNT >= 1
    ENTITY_CALENDAR_1,
#endif
#if ENTITY_CALENDARS_COUNT >= 2
    ENTITY_CALENDAR_2,
#endif
#if ENTITY_CALENDARS_COUNT >= 3
    ENTITY_CALENDAR_3,
#endif
#if ENTITY_CALENDARS_COUNT >= 4
    ENTITY_CALENDAR_4,
#endif
#if ENTITY_CALENDARS_COUNT >= 5
    ENTITY_CALENDAR_5,
#endif
};

// Update intervals (ms)
const unsigned long WEATHER_UPDATE_INTERVAL_MS = 60UL * 60UL * 1000UL; // 1 hour
const unsigned long CAL_TODO_UPDATE_INTERVAL_MS =
    6UL * 60UL * 60UL * 1000UL; // 6 hours
const unsigned long QUOTE_ROTATION_INTERVAL_MS =
    6UL * 60UL * 60UL * 1000UL; // 6 hours
const unsigned long QUOTE_FETCH_INTERVAL_MS =
    24UL * 60UL * 60UL * 1000UL; // 24 hours (fetch new quotes once per day)
const unsigned long MIDNIGHT_CHECK_INTERVAL_MS = 60UL * 1000UL; // check once per minute
const unsigned long OTA_WINDOW_MS = 5UL * 60UL * 1000UL; // OTA enabled for 5 minutes after button press

// ---------- Data Structures ----------
struct WeatherData {
  String temperature;
  String condition;
};

struct TodoItem {
  String text;
  bool completed;
};

struct CalendarEvent {
  String title;
  String startTime;   // formatted string (HH:MM)
  String date;        // formatted string (MM-DD)
  String isoDateTime; // Original ISO datetime for countdown calculation
};

struct QuoteData {
  String author;
  String text;
};

// Battery data structure and constants
struct BatteryData {
  float voltage;
  int percentage;
  bool isCharging;
};
BatteryData batteryInfo = {0.0, 0, false};
const unsigned long BATTERY_UPDATE_INTERVAL_MS =
    10UL * 60UL * 1000UL; // Update every 10 minutes
int vref = 1100;   // Reference voltage in mV (will be calibrated from eFuse if
                   // available)

// Global Data
WeatherData currentWeather;
std::vector<TodoItem> todoList;
std::vector<CalendarEvent> calendarEvents;
std::vector<QuoteData> quotes;
int currentQuoteIndex = 0;

// Initialize STL collections with reasonable reserved capacity to reduce heap fragmentation
void initCollections() {
  todoList.reserve(8);          // Tasks due today are usually small in number
  calendarEvents.reserve(16);   // A week of events across calendars
  quotes.reserve(64);           // Daily quotes cache
}

// ---------- Layout ----------
// Screen is 960x540
// Layout:
//   ┌──────────────────────────────────────┐
//   │  LARGE TIME          DATE            │  <- Top header (full width)
//   ├────────────────┬─────────────────────┤
//   │  WEATHER       │   TODO LIST         │  <- Middle section (split)
//   │  [ICON]        │   □ Task 1          │
//   │  Temp          │   □ Task 2          │
//   │  Condition     │   ☑ Task 3          │
//   ├────────────────┴─────────────────────┤
//   │  CALENDAR / UPCOMING EVENTS          │  <- Bottom section (full width)
//   │  • Event 1 - Time                    │
//   └──────────────────────────────────────┘

// Top Header - Date, WiFi, Weather (Screen: 960x540)
const Rect_t clockArea = {.x = 20,
                          .y = 20,
                          .width = 120,
                          .height = 35}; // Used for date display (clock removed)
const Rect_t weatherArea = {.x = 820,
                            .y = 20,
                            .width = 120,
                            .height = 35}; // Weather in top right
// const Rect_t dateArea = {.x = 420,
//                          .y = 20,
//                          .width = 120,
//                          .height = 35}; // Unused slot (left empty)
const Rect_t wifiStatusArea = {
    .x = 260, .y = 20, .width = 60, .height = 35}; // WiFi status between date and weather
// const Rect_t batteryArea = {.x = 220,
//                             .y = 20,
//                             .width = 120,
//                             .height = 35}; // (not drawn) battery indicator area

// Quote Section - Between header and todo (full width, single line)
const Rect_t quoteArea = {
    .x = 20,
    .y = 60,
    .width = 920,
    .height = 28}; // Full width for daily quote (quote only, no author)

// Middle Section - Todo (left half) and UPCOMING Calendar (right half) side by
// side. Content spans roughly y=105..420 for tighter vertical fit.
const Rect_t todoHeaderArea = {
    .x = 20, .y = 105, .width = 450, .height = 40}; // Left half
const Rect_t todoListArea = {
    .x = 20,
    .y = 145,
    .width = 450,
    .height = 290}; // Left half - ends near y=435, closer to divider
const Rect_t calendarHeaderArea = {
    .x = 490, .y = 105, .width = 450, .height = 40}; // Right half
const Rect_t calendarListArea = {
    .x = 490,
    .y = 145,
    .width = 450,
    .height = 290}; // Right half - ends near y=435, closer to divider
// Bottom Section - Mini Calendar (compact, full width)
// Screen is 960x540, so we need to ensure it fits within bounds
// Need enough height for 2 rows: day labels (~30px) + spacing + day numbers
// (~30px) Positioned at bottom: y=490 to y=540 (50px height)
const Rect_t miniCalendarArea = {
    .x = 20,
    .y = 470,
    .width = 920,
    .height = 60}; // Compact mini calendar at bottom, fits within 540px

// ---------- Globals ----------
unsigned long lastWeatherUpdate = 0;
unsigned long lastCalTodoUpdate = 0;
unsigned long lastQuoteFetch = 0;
unsigned long lastQuoteRotation = 0;
unsigned long lastBatteryUpdate = 0;
unsigned long lastMidnightCheck = 0;
String currentDateString = ""; // Track current date for logging and comparisons
// uint32_t weatherFailCount = 0;
// uint32_t todoFailCount = 0;
// uint32_t calFailCount = 0;
// uint32_t quoteFailCount = 0;
bool fullRefreshScheduled = false;
int lastMidnightDay = -1;
bool otaEnabled = false;
unsigned long otaWindowEnds = 0;
int lastButtonState = HIGH;
unsigned long wifiLingerUntil = 0; // keep WiFi up briefly after fetches

bool debugMode = false; // true when BUTTON_1 held at boot, keeps device awake for OTA/debug


// ---------- WiFi helpers ----------
void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED)
    return;

  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

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

void forceWifiOff() {
  Serial.println("Forcing WiFi off");
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  wifiLingerUntil = 0;
}

// Build base URL for HA with protocol selection
String buildHaUrl(const String &path) {
  String scheme = HA_USE_HTTPS ? "https" : "http";
  String url = scheme + "://" + HA_HOST + ":" + HA_PORT + path;
  return url;
}

// ---------- Home Assistant REST API helpers ----------
bool ensureWiFi() {
  if (WiFi.status() == WL_CONNECTED)
    return true;
  connectWiFi();
  return WiFi.status() == WL_CONNECTED;
}

/**
 * Generic helper to fetch JSON from Home Assistant REST API
 *
 * @param url Full URL to HA REST API endpoint
 * @param doc JsonDocument to populate with response data
 * @return true if successful, false otherwise
 */
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
    http.addHeader("Authorization", String("Bearer ") + HA_TOKEN);
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

void fetchWeather() {
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

void fetchQuotes() {
  Serial.println("=== fetchQuotes() called ===");
  String url = buildHaUrl(String("/api/states/") + String(ENTITY_QUOTE));
  Serial.print("Quote URL: ");
  Serial.println(url);

  haArrayDoc.clear();

  if (!fetchJson(url, haArrayDoc)) {
    Serial.println("ERROR: Quote fetch failed - fetchJson returned false");
    return;
  }

  Serial.println("Quote JSON fetched successfully");

  // Clear existing quotes
  quotes.clear();

  // Parse the quotes/entries array from attributes (accept both keys)
  JsonArray entries = haArrayDoc["attributes"]["quotes"].as<JsonArray>();
  if (entries.isNull()) {
    entries = haArrayDoc["attributes"]["entries"].as<JsonArray>();
  }
  if (entries.isNull()) {
    Serial.println("ERROR: No 'quotes' or 'entries' array found in attributes");
    return;
  }

  Serial.print("Found ");
  Serial.print(entries.size());
  Serial.println(" quotes");

  // Parse each entry
  for (JsonObject entry : entries) {
    QuoteData quote;

    // Get author from title
    const char *author = entry["title"];
    if (author) {
      quote.author = String(author);
    } else {
      quote.author = "Unknown";
    }

    // Get quote text from summary (remove surrounding quotes if present)
    const char *summary = entry["summary"];
    if (summary) {
      String text = String(summary);
      // Remove leading/trailing quotes and backslashes
      text.trim();
      if (text.startsWith("\"") && text.endsWith("\"")) {
        text = text.substring(1, text.length() - 1);
      }
      // Remove escaped quotes
      text.replace("\\\"", "\"");
      quote.text = text;
    } else {
      quote.text = "";
    }

    // Only add if we have valid text
    if (quote.text.length() > 0) {
      quotes.push_back(quote);
      Serial.print("Added quote from ");
      Serial.print(quote.author);
      Serial.print(": ");
      Serial.println(quote.text.substring(0, 50)); // Print first 50 chars
    }
  }

  Serial.print("Total quotes stored: ");
  Serial.println(quotes.size());

  // Reset current quote index
  currentQuoteIndex = 0;

  Serial.println("=== fetchQuotes() complete ===");
  disableWiFiIfAllowed();
}

// Helper to get today's date string (YYYY-MM-DD) from NTP
String getTodayDateString() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return "";
  }
  char timeStringBuff[20];
  strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%d", &timeinfo);
  return String(timeStringBuff);
}

// Helper to fetch JSON via POST (for Service Calls)
/**
 * POST JSON data to Home Assistant REST API
 * Used for service calls (e.g., todo list operations)
 *
 * @param url Full URL to HA REST API endpoint
 * @param payload JSON payload string
 * @param doc JsonDocument to populate with response data
 * @return true if successful, false otherwise
 */
bool fetchJsonPost(const String &url, const String &payload,
                   JsonDocument &doc) {
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
  http.addHeader("Authorization", String("Bearer ") + HA_TOKEN);
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

void fetchTodos() {
  // 1. Get current date for filtering
  String today = getTodayDateString();
  if (today.length() == 0) {
    return;
  }

  std::vector<TodoItem> newTodos;
  haDoc.clear(); // Reuse shared document for service response

  for (const char *entity : ENTITY_TODOS) {
    String url = buildHaUrl(
        "/api/services/todo/get_items?return_response=true");
    // Add status: needs_action to be explicit and match common usage
    String payload = String("{\"entity_id\": \"") + entity +
                     "\", \"status\": \"needs_action\"}";

    haDoc.clear();

    if (!fetchJsonPost(url, payload, haDoc))
      continue;

    // Service response structure (with return_response=true):
    // {
    //   "service_response": {
    //     "todo.errands": {
    //       "items": [ ... ]
    //     }
    //   }
    // }

    JsonArray items = haDoc["service_response"][entity]["items"];
    if (items.isNull()) {
      continue;
    }

    for (JsonVariant v : items) {
      String summary = v["summary"].as<String>();
      String status = v["status"].as<String>();

      // Skip completed items
      if (status == "completed") {
        continue;
      }

      // ONLY show items with a due date that is TODAY (not past dates)
      if (v["due"].isNull()) {
        // Skip items without due dates
        continue;
      }

      String due = v["due"].as<String>();

      // Keep if due is valid AND due date equals today (exact match)
      if (due.length() >= 10 && due.substring(0, 10) == today) {
        TodoItem item;
        item.text = summary;
        item.completed = false;
        newTodos.push_back(item);
      }
    }
  }

  // Limit to 6 items
  if (newTodos.size() > 6) {
    newTodos.resize(6);
  }

  if (!newTodos.empty()) {
    todoList = newTodos;
  }
  disableWiFiIfAllowed();
}

// Helper to get URL-encoded ISO8601 string for Calendar API
String getISOTime(time_t t) {
  struct tm *tm = localtime(&t);
  char buf[30];
  // Format: YYYY-MM-DDTHH:MM:SS
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", tm);
  return String(buf);
}

/**
 * Fetch calendar events from Home Assistant
 * Gets events from multiple calendar entities for the next 7 days
 * Sorts events chronologically and limits to 10 events
 */
void fetchCalendar() {
  Serial.println("=== fetchCalendar() called ===");
  std::vector<CalendarEvent> newEvents;
  haArrayDoc.clear();

  // Get current time and end time (7 days later)
  time_t now;
  time(&now);
  time_t end = now + (7 * 24 * 60 * 60);

  String startStr = getISOTime(now);
  String endStr = getISOTime(end);

  Serial.print("Calendar time range: ");
  Serial.print(startStr);
  Serial.print(" to ");
  Serial.println(endStr);

  // URL Encode the timestamps (specifically :)
  startStr.replace(":", "%3A");
  endStr.replace(":", "%3A");

  Serial.print("Number of calendar entities configured: ");
  Serial.println(ENTITY_CALENDARS.size());

  int totalEvents = 0;
  for (const char *entity : ENTITY_CALENDARS) {
    Serial.println("");
    Serial.print(">>> Processing calendar entity: ");
    Serial.println(entity);
    // Use the Calendar API to get events in range
    // URL: /api/calendars/{entity}?start={start}&end={end}
    String url = buildHaUrl(String("/api/calendars/") + entity + "?start=" +
                            startStr + "&end=" + endStr);

    Serial.print("Fetching Calendar URL: ");
    Serial.println(url);
    haArrayDoc.clear();

    if (!fetchJson(url, haArrayDoc)) {
      Serial.print("ERROR: Failed to fetch calendar: ");
      Serial.println(entity);
      Serial.print("URL was: ");
      Serial.println(url);
      Serial.println(">>> Moving to next calendar entity");
      continue;
    }

    Serial.println(">>> fetchJson succeeded");

    // Check if we got valid calendar data
    if (!haArrayDoc.is<JsonArray>()) {
      Serial.print("WARNING: Calendar response was not an array for entity: ");
      Serial.println(entity);
      Serial.print("Response type: ");
      if (haArrayDoc.is<JsonObject>()) {
        Serial.println("Object (unexpected)");
        serializeJson(haArrayDoc, Serial);
        Serial.println();
      } else {
        Serial.println("Unknown");
      }
      continue;
    }

    Serial.print("Calendar JSON fetched successfully for: ");
    Serial.println(entity);

    // The API returns a JSON Array of events directly
    if (haArrayDoc.is<JsonArray>()) {
      JsonArray events = haArrayDoc.as<JsonArray>();
      Serial.print(">>> Found ");
      Serial.print(events.size());
      Serial.print(" events in ");
      Serial.println(entity);

      if (events.size() == 0) {
        Serial.println(">>> No events in this calendar (empty array)");
      }

      for (JsonVariant v : events) {
        CalendarEvent evt;

        // "start": { "dateTime": "..." } or { "date": "..." }
        if (v["start"]["dateTime"].is<String>()) {
          evt.isoDateTime = v["start"]["dateTime"].as<String>();
        } else if (v["start"]["date"].is<String>()) {
          evt.isoDateTime = v["start"]["date"].as<String>(); // All day
        }

        evt.title = v["summary"].as<String>();
        newEvents.push_back(evt);
        totalEvents++;

        Serial.print("  Event: ");
        Serial.print(evt.title);
        Serial.print(" at ");
        Serial.println(evt.isoDateTime);
      }
    } else {
      Serial.print("ERROR: Calendar response was not an array for ");
      Serial.println(entity);
      Serial.print("Response type: ");
      if (haArrayDoc.is<JsonObject>()) {
        Serial.println("Object");
        serializeJson(haArrayDoc, Serial);
        Serial.println();
      } else {
        Serial.println("Unknown");
      }
    }
  }

  Serial.print("Total calendar events fetched: ");
  Serial.println(totalEvents);

  // Format for display BEFORE sorting and limiting
  for (auto &evt : newEvents) {
    if (evt.isoDateTime.length() >= 10) {
      // Simple parsing assuming ISO format YYYY-MM-DD...
      evt.date = evt.isoDateTime.substring(5, 10); // MM-DD
      if (evt.isoDateTime.length() >= 16) {
        evt.startTime = evt.isoDateTime.substring(11, 16); // HH:MM
      } else {
        evt.startTime = "All Day";
      }
    }
  }

  // Sort by ISO datetime FIRST (to get chronological order)
  std::sort(newEvents.begin(), newEvents.end(),
            [](const CalendarEvent &a, const CalendarEvent &b) {
              return a.isoDateTime < b.isoDateTime;
            });

  // NOW limit to display area (increase to 10 items to show more events for the
  // week) Each event takes 2 lines (date/time + title), so 10 items = 5 events
  if (newEvents.size() > 10) {
    newEvents.resize(10);
  }

  Serial.print("Calendar events after sorting and limiting: ");
  Serial.println(newEvents.size());

  Serial.print("Final calendar events count after formatting: ");
  Serial.println(newEvents.size());
  Serial.println("=== fetchCalendar() complete ===");

  if (!newEvents.empty()) {
    calendarEvents = newEvents;
  }
  disableWiFiIfAllowed();
}


// ---------- Icon Drawing Functions ----------
// Draw bitmap icons using pre-defined data
void drawBitmapIcon(int32_t x, int32_t y, const uint8_t *icon_data,
                    uint32_t width, uint32_t height) {
  Rect_t icon_area = {
      .x = x, .y = y, .width = (int32_t)width, .height = (int32_t)height};
  epd_draw_image(icon_area, (uint8_t *)icon_data, BLACK_ON_WHITE);
}

// ---------- Drawing helpers ----------
// Draw a text-based divider line using dashes
void drawTextDivider(int32_t x, int32_t y, int32_t width) {
  // Create a string of dashes (approximately 1 dash per 10 pixels for
  // visibility)
  int32_t numDashes = width / 10;
  if (numDashes < 1)
    numDashes = 1;

  String dividerText = "";
  for (int32_t i = 0; i < numDashes; i++) {
    dividerText += "-";
  }

  // Draw the divider text
  int32_t cursor_x = x;
  int32_t cursor_y = y + FiraSans.advance_y + FiraSans.descender;
  writeln((GFXfont *)&FiraSans, dividerText.c_str(), &cursor_x, &cursor_y,
          NULL);
}

/**
 * Draw daily motivational quote on the display
 * Uses FiraSansSmall font, truncates if too long
 * Clears quote area before drawing
 */
void drawQuote() {
  if (quotes.empty()) {
    Serial.println("No quotes available");
    return;
  }

  // Get current quote
  QuoteData currentQuote = quotes[currentQuoteIndex];

  // Format quote text with quotes (no author)
  String quoteText = "\"" + currentQuote.text + "\"";

  // Truncate quote if too long.
  // With the smaller font we can fit more text, but extremely long quotes can still
  // overrun the line, so keep a generous but safe limit.
  const int MAX_QUOTE_CHARS = 110;
  if (quoteText.length() > MAX_QUOTE_CHARS) {
    quoteText = quoteText.substring(0, MAX_QUOTE_CHARS - 3) + "...";
  }

  Serial.print("Drawing quote: ");
  Serial.println(quoteText);

  // Clear and draw quote (single line, no author)
  // Clear the quote area (don't extend beyond screen bounds - screen is 960px
  // wide)
  epd_clear_area(quoteArea);
  int32_t cursor_x = quoteArea.x;
  int32_t cursor_y =
      quoteArea.y + FiraSansSmall.advance_y + FiraSansSmall.descender;

  // Draw quote text only
  writeln((GFXfont *)&FiraSansSmall, quoteText.c_str(), &cursor_x, &cursor_y,
          NULL);
}

// Rotate to next quote
void rotateQuote() {
  if (quotes.empty()) {
    return;
  }

  currentQuoteIndex++;
  if (currentQuoteIndex >= (int)quotes.size()) {
    currentQuoteIndex = 0;
  }

  Serial.print("Rotating to quote index: ");
  Serial.println(currentQuoteIndex);

  // Redraw the quote section
  epd_poweron();
  drawQuote();
  epd_poweroff();
}

/**
 * Check if WiFi is currently connected
 * @return true if connected, false otherwise
 */
bool isWiFiConnected() { return (WiFi.status() == WL_CONNECTED); }

void disableWiFiIfAllowed() {
  // In debug mode, keep WiFi on so OTA and live debugging remain available
  if (debugMode)
    return;
  if (otaEnabled)
    return;
  if (millis() < wifiLingerUntil)
    return;
  forceWifiOff();
}

void enableOtaWindow() {
  otaEnabled = true;
  otaWindowEnds = millis() + OTA_WINDOW_MS;
  Serial.println("OTA window enabled");
}

/**
 * Draw WiFi status icon (connected or disconnected)
 *
 * @param x Left position
 * @param y Top position
 */
void drawWiFiStatus(int32_t x, int32_t y) {
  if (isWiFiConnected()) {
    drawBitmapIcon(x, y, icon_wifi_connected_data, icon_wifi_connected_width,
                   icon_wifi_connected_height);
  } else {
    drawBitmapIcon(x, y, icon_wifi_disconnected_data,
                   icon_wifi_disconnected_width, icon_wifi_disconnected_height);
  }
}

/**
 * Read battery voltage and calculate percentage
 * Note: This uses the ADC directly and does not depend on EPD power state.
 *
 * @return BatteryData struct with voltage, percentage, and charging status
 */
BatteryData readBattery() {
  BatteryData bat;

  // Read ADC value (0-4095 for 12-bit ADC)
  uint16_t adcValue = analogRead(BATT_PIN);

  // Debug: Print raw ADC value
  Serial.print("  Raw ADC value: ");
  Serial.println(adcValue);

  // Calculate voltage: ADC value / 4095 * 2.0 (voltage divider) * 3.3V *
  // (vref/1000) Formula from demo example: ((float)v / 4095.0) * 2.0 * 3.3 *
  // (vref / 1000.0)
  float voltage = ((float)adcValue / 4095.0) * 2.0 * 3.3 * (vref / 1000.0);

  // Debug: Print calculated voltage before clamping
  Serial.print("  Calculated voltage (before clamp): ");
  Serial.print(voltage, 3);
  Serial.println("V");

  // Clamp to max 4.2V (fully charged LiPo)
  if (voltage >= 4.2) {
    voltage = 4.2;
  }

  // Check for invalid readings (too low or zero)
  if (voltage < 0.5) {
    Serial.println("  ⚠ WARNING: Battery voltage very low (< 0.5V) - Check "
                   "battery connection!");
    Serial.println("  ⚠ Possible issues:");
    Serial.println("     - Battery not connected");
    Serial.println("     - Battery completely discharged");
    Serial.println("     - ADC pin issue");
  }

  bat.voltage = voltage;

  // Calculate percentage using a more realistic range:
  // ~3.3V  = 0%
  // ~4.15V = 100%
  //
  // Note:
  // - Many LiPo packs sit near 4.2V while still charging (constant voltage
  //   phase), so mapping 4.2V directly to 100% tends to show "100%" too early.
  // - Using 3.3–4.15V gives a more useful spread of percentages in normal use.
  float minVoltage = 3.3;  // Treat ~3.3V as effectively empty for display purposes
  float maxVoltage = 4.15; // Treat ~4.15V as "full" for percentage calculation

  // Only calculate percentage if voltage is reasonable
  if (voltage >= minVoltage) {
    bat.percentage =
        (int)(((voltage - minVoltage) / (maxVoltage - minVoltage)) * 100.0);
    bat.percentage = constrain(bat.percentage, 0, 100);
  } else if (voltage > 0.5) {
    // Voltage between 0.5V and 3.0V - battery is very low but connected
    // Show a small percentage instead of 0% to indicate battery is present
    bat.percentage =
        (int)((voltage / minVoltage) * 5.0); // Show 0-5% for very low battery
    Serial.print("  ⚠ Battery voltage very low (");
    Serial.print(voltage, 3);
    Serial.print("V) - Battery needs charging!");
  } else {
    // Voltage below 0.5V - battery likely not connected or completely dead
    bat.percentage = 0;
    Serial.print("  ⚠ Voltage (");
    Serial.print(voltage, 3);
    Serial.print("V) below minimum (");
    Serial.print(minVoltage);
    Serial.println("V) - Check battery connection!");
  }

  // Charging detection no longer displayed
  bat.isCharging = false;

  return bat;
}



// Parse ISO datetime string to time_t
time_t parseISODateTime(const String &isoStr) {
  if (isoStr.length() < 10)
    return 0;

  // Work on a trimmed copy that strips timezone information (Z, +HH:MM, -HH:MM)
  String trimmed = isoStr;

  int tzPos = trimmed.indexOf('Z');
  if (tzPos == -1) {
    // Look for '+' or '-' only after the date portion (index > 10),
    // to avoid matching the '-' in "YYYY-MM-DD"
    for (int i = 10; i < (int)trimmed.length(); ++i) {
      char c = trimmed.charAt(i);
      if (c == '+' || c == '-') {
        tzPos = i;
        break;
      }
    }
  }
  if (tzPos > 10) {
    trimmed = trimmed.substring(0, tzPos);
  }

  struct tm timeinfo = {0};
  int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;

  // First try full datetime: YYYY-MM-DDTHH:MM:SS
  int parsed = sscanf(trimmed.c_str(), "%d-%d-%dT%d:%d:%d",
                      &year, &month, &day, &hour, &minute, &second);
  if (parsed < 3) {
    // Fallback to date-only: YYYY-MM-DD
    hour = minute = second = 0;
    parsed = sscanf(trimmed.c_str(), "%d-%d-%d", &year, &month, &day);
  }

  if (parsed >= 3) {
    timeinfo.tm_year = year - 1900;
    timeinfo.tm_mon = month - 1;
    timeinfo.tm_mday = day;
    timeinfo.tm_hour = hour;
    timeinfo.tm_min = minute;
    timeinfo.tm_sec = second;
    return mktime(&timeinfo);
  }

  return 0;
}

// Calculate time until next calendar event (returns minutes, -1 if no events)
int getMinutesUntilNextEvent() {
  if (calendarEvents.empty())
    return -1;

  time_t now;
  time(&now);

  // Find the next event in the future
  for (const auto &evt : calendarEvents) {
    time_t eventTime = parseISODateTime(evt.isoDateTime);
    if (eventTime > now) {
      int diff = (int)((eventTime - now) / 60); // Convert to minutes
      return diff;
    }
  }

  return -1; // No future events
}



// Draw mini calendar month view - shows current week (compact with small font)
void drawMiniCalendar(int32_t x, int32_t y, int32_t width, int32_t height) {
  time_t now;
  time(&now);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);

  // Get current day and weekday
  int currentDay = timeinfo.tm_mday;
  int currentWeekday = timeinfo.tm_wday; // 0 = Sunday, 6 = Saturday

  // Calculate start of week (Sunday = 0)
  int startDay = currentDay - currentWeekday;

  // Get days in current month
  int currentMonth = timeinfo.tm_mon + 1;
  int currentYear = timeinfo.tm_year + 1900;
  int daysInMonth = 31;
  if (currentMonth == 4 || currentMonth == 6 || currentMonth == 9 ||
      currentMonth == 11) {
    daysInMonth = 30;
  } else if (currentMonth == 2) {
    daysInMonth = ((currentYear % 4 == 0 && currentYear % 100 != 0) ||
                   (currentYear % 400 == 0))
                      ? 29
                      : 28;
  }

  // Adjust if startDay is before month start
  if (startDay < 1) {
    startDay = 1;
  }

  // Compact layout: day labels and numbers on same line, using small font
  const char *dayLabels = "SMTWTFS";
  int32_t dayWidth = width / 7;

  // Draw day labels (S M T W T F S) - small font, top row
  // Start at the very top of the area (y=495)
  int32_t cursor_y = y + FiraSansSmall.advance_y + FiraSansSmall.descender;
  for (int i = 0; i < 7; i++) {
    int32_t cursor_x = x + (i * dayWidth) + (dayWidth / 2) - 2;
    char label[2] = {dayLabels[i], '\0'};
    writeln((GFXfont *)&FiraSansSmall, label, &cursor_x, &cursor_y, NULL);
  }

  // Draw day numbers for current week (7 days) - small font, bottom row
  // Position below day labels with proper spacing to avoid overlap
  // Area is y=490, height=50, so max y=540
  // Day labels use ~30px (advance_y), so start numbers at y + 30 + spacing
  cursor_y = y + FiraSansSmall.advance_y +
             18; // 18px gap between labels and numbers for better readability

  for (int i = 0; i < 7; i++) {
    int day = startDay + i;
    if (day > daysInMonth)
      break; // Past end of month

    int32_t cursor_x =
        x + (i * dayWidth) + (dayWidth / 2) - 3; // Center day number

    // Highlight today with a box border
    bool isToday = (day == currentDay);

    // Draw day number
    const GFXfont *dayFont = (GFXfont *)&FiraSansSmall;
    int32_t num_y = cursor_y;
    char dayStr[4];
    snprintf(dayStr, sizeof(dayStr), "%d", day);
    writeln(dayFont, dayStr, &cursor_x, &num_y, NULL);
  }
}

void drawText(const Rect_t &area, String text, bool alignRight = false) {
  int32_t cursor_x = area.x;
  int32_t cursor_y = area.y + FiraSans.advance_y + FiraSans.descender;

  if (alignRight) {
    // Simple right align estimation (not perfect without measuring)
    // For now, let's just stick to left align or center if needed.
    // Actually, let's just ignore alignRight for simplicity unless we
    // measure text.
  }

  epd_clear_area(area);
  // Truncate text if too long to prevent cutoff
  String displayText = text;
  if (displayText.length() > 60) { // Approximate max chars for weather area
    displayText = displayText.substring(0, 57) + "...";
  }
  writeln((GFXfont *)&FiraSans, displayText.c_str(), &cursor_x, &cursor_y,
          NULL);
}

void drawList(const Rect_t &area, const std::vector<String> &lines,
              String header, bool drawIcons = false, int lineSpacing = 35,
              const uint8_t *headerIconData = NULL,
              uint32_t headerIconWidth = 0, uint32_t headerIconHeight = 0) {
  epd_clear_area(area);

  int32_t cursor_x = area.x;
  int32_t cursor_y = area.y + FiraSans.advance_y + FiraSans.descender;

  // Draw Header (only if provided and non-empty)
  if (header.length() > 0) {
    // Draw header icon if provided
    if (headerIconData != NULL && headerIconWidth > 0 && headerIconHeight > 0) {
      int32_t icon_x = cursor_x;
      int32_t icon_y = cursor_y - headerIconHeight -
                       2; // Position icon slightly above text baseline
      drawBitmapIcon(icon_x, icon_y, headerIconData, headerIconWidth,
                     headerIconHeight);
      cursor_x += headerIconWidth + 5; // Space after icon
    }

    writeln((GFXfont *)&FiraSansMedium, header.c_str(), &cursor_x, &cursor_y,
            NULL);

    // If this is UPCOMING header, add countdown next to it
    if (header == "UPCOMING") {
      cursor_x += 10; // Space between header and countdown
      int minutesUntilNext = getMinutesUntilNextEvent();
      if (minutesUntilNext >= 0) {
        int hours = minutesUntilNext / 60;
        int mins = minutesUntilNext % 60;
        char countdownStr[20];
        if (hours > 0) {
          snprintf(countdownStr, sizeof(countdownStr), "(Next: %dh %dm)", hours,
                   mins);
        } else {
          snprintf(countdownStr, sizeof(countdownStr), "(Next: %dm)", mins);
        }
        writeln((GFXfont *)&FiraSansSmall, countdownStr, &cursor_x, &cursor_y,
                NULL);
      }
    }

    cursor_y += 35; // Increased header spacing to separate header from items
  }

  // Draw Items with word wrapping consideration
  int itemIndex = 0;
  for (const String &line : lines) {
    cursor_x = area.x;

    // Draw icon if needed (for todos)
    if (drawIcons && itemIndex < (int)lines.size()) {
      bool isChecked = (line.length() > 0 && line[0] == 'X');
      int32_t icon_x = cursor_x;
      int32_t icon_y = cursor_y - icon_checkbox_height - 2;

      // Draw checkbox icon
      if (isChecked) {
        drawBitmapIcon(icon_x, icon_y, icon_checkbox_checked_data,
                       icon_checkbox_checked_width,
                       icon_checkbox_checked_height);
      } else {
        drawBitmapIcon(icon_x, icon_y, icon_checkbox_data, icon_checkbox_width,
                       icon_checkbox_height);
      }

      cursor_x += icon_checkbox_width + 5; // Space after icon
    }

    // Truncate long lines to fit in area width
    String displayLine = line;

    // Store original line for detection (before any modifications)
    String originalLine = line;

    // Remove icon prefix if present
    if (displayLine.length() > 2 &&
        (displayLine[0] == 'X' || displayLine[0] == '>')) {
      displayLine = displayLine.substring(2);
    }
    // Adjust max length based on area width (450px = ~28 chars, 920px = ~50
    // chars)
    int maxChars = (area.width < 500)
                       ? 28
                       : 50; // Half width gets 28 chars, full width gets 50
    if (displayLine.length() > maxChars) {
      displayLine = displayLine.substring(0, maxChars - 3) + "...";
    }

    // Check if this is a date/time line (starts with date pattern like "1/15"
    // or contains "All Day") Use ORIGINAL line (before truncation) for
    // detection
    bool isDateTimeLine = false;

    // More robust check: line must start with a digit and contain "/" within
    // first 6 chars OR contain "All Day" Also check: lines that start with
    // spaces are NOT date/time (they're indented titles)
    if (originalLine.indexOf("All Day") >= 0) {
      isDateTimeLine = true;
    } else if (originalLine.length() > 0) {
      // Check if first character is a digit (month) - NOT a space
      char firstChar = originalLine.charAt(0);
      if (firstChar >= '0' && firstChar <= '9') {
        // Now check if there's a "/" within first 6 characters (covers "1/15",
        // "10/15", "12/1")
        int slashPos = originalLine.indexOf('/');
        if (slashPos >= 1 && slashPos <= 5) {
          isDateTimeLine = true;
        }
      }
    }

    // Debug logging for UPCOMING events
    if (!drawIcons &&
        itemIndex < 6) { // Log first 6 items to see date/time pairs
      Serial.print("Line ");
      Serial.print(itemIndex);
      Serial.print(": original='");
      Serial.print(
          originalLine.substring(0, min(20, (int)originalLine.length())));
      Serial.print("' display='");
      Serial.print(
          displayLine.substring(0, min(20, (int)displayLine.length())));
      Serial.print("' isDateTime=");
      Serial.println(isDateTimeLine);
    }

    // Use smaller font for date/time lines in calendar (not for todo items)
    if (isDateTimeLine && !drawIcons) {
      writeln((GFXfont *)&FiraSansSmall, displayLine.c_str(), &cursor_x,
              &cursor_y, NULL);
      // Smaller font needs adjustment - add spacing to prevent overlap with
      // title
      cursor_y +=
          lineSpacing + 2; // Add extra spacing after small date/time line
    } else {
      writeln((GFXfont *)&FiraSansMedium, displayLine.c_str(), &cursor_x,
              &cursor_y, NULL);
      // Normal spacing for all other lines
      cursor_y += lineSpacing;
    }

    // Check if next line would overflow (check BEFORE incrementing for next
    // line)
    int nextY = cursor_y + lineSpacing;
    int maxY = area.y + area.height;
    Serial.print("drawList: After line ");
    Serial.print(itemIndex);
    Serial.print(", cursor_y=");
    Serial.print(cursor_y);
    Serial.print(", nextY would be=");
    Serial.print(nextY);
    Serial.print(", maxY=");
    Serial.print(maxY);
    Serial.print(", remaining=");
    Serial.print(maxY - cursor_y);
    Serial.println("px");

    if (nextY > maxY) {
      Serial.print(">>> STOPPING at item ");
      Serial.print(itemIndex);
      Serial.print(" - nextY (");
      Serial.print(nextY);
      Serial.print(") > maxY (");
      Serial.print(maxY);
      Serial.println(")");
      break;
    }
    itemIndex++;
  }

  Serial.print("drawList: Completed - drew ");
  Serial.print(itemIndex);
  Serial.print(" out of ");
  Serial.print(lines.size());
  Serial.println(" lines");
}

// Update weather section only - compact display for top center with icon
void updateWeatherSection(bool powerOn = true) {
  Serial.println("Updating Weather Section...");
  if (powerOn) {
    epd_poweron();
  }
  epd_clear_area(weatherArea);

  // Draw weather icon on the left (smaller size to fit compact area)
  String condition = currentWeather.condition;
  int32_t icon_x = weatherArea.x + 2;
  int32_t icon_y =
      weatherArea.y - 6; // Slightly above to center better in 35px height

  // Use smaller icon size - draw 32x32 from 48x48 icon (centered)
  if (condition.indexOf("sun") >= 0 || condition.indexOf("clear") >= 0) {
    // Draw sun icon - use a smaller area (32x32) from the 48x48 icon
    Rect_t icon_area = {.x = icon_x, .y = icon_y, .width = 32, .height = 32};
    // We'll draw the full icon but it will be clipped to the area
    drawBitmapIcon(icon_x, icon_y, icon_sun_data, icon_sun_width,
                   icon_sun_height);
  } else {
    // Draw cloud icon
    drawBitmapIcon(icon_x, icon_y, icon_cloud_data, icon_cloud_width,
                   icon_cloud_height);
  }

  // Compact display: show temperature next to icon (e.g., "22°C" or "72°F")
  // Extract just the number from temperature string (remove " C" or " F")
  String tempDisplay = currentWeather.temperature;
  // Remove trailing space and unit if present, we'll add our own
  tempDisplay.trim();
  if (tempDisplay.endsWith(" C")) {
    tempDisplay = tempDisplay.substring(0, tempDisplay.length() - 2) + "°C";
  } else if (tempDisplay.endsWith(" F")) {
    tempDisplay = tempDisplay.substring(0, tempDisplay.length() - 2) + "°F";
  }

  // Draw temperature text next to icon (starting after icon + larger gap) -
  // medium font
  int32_t weather_x =
      weatherArea.x +
      50; // Start after icon (48px) + 2px gap for better spacing
  int32_t weather_y =
      weatherArea.y + 25; // Same offset as clock/date for alignment
  writeln((GFXfont *)&FiraSansMedium, tempDisplay.c_str(), &weather_x,
          &weather_y, NULL);

  if (powerOn) {
    epd_poweroff();
  }
}

// Update calendar and todo sections only
void updateCalendarTodoSections() {
  Serial.println("Updating Calendar and Todo Sections...");
  epd_poweron();

  // Update Todo List
  std::vector<String> todoLines;
  todoLines.reserve(todoList.size() + 1);
  for (const auto &item : todoList) {
    String prefix = item.completed ? "X " : "> ";
    todoLines.push_back(prefix + item.text);
  }

  if (todoLines.empty()) {
    todoLines.push_back("X Nothing due today");
  }

  drawList(todoListArea, todoLines, "TODO", true, 35, icon_todo_data,
           icon_todo_width, icon_todo_height);

  // Update Calendar Events
  std::vector<String> calLines;
  calLines.reserve(calendarEvents.size() * 2 + 1); // two lines per event + fallback
  for (const auto &evt : calendarEvents) {
    String eventLine = evt.date + " " + evt.startTime + " - " + evt.title;
    if (eventLine.length() > 90) {
      eventLine = eventLine.substring(0, 87) + "...";
    }
    calLines.push_back(eventLine);
  }

  if (calLines.empty()) {
    calLines.push_back("No upcoming events");
  }

  // Use tighter line spacing (28) for calendar to fit more events
  drawList(calendarListArea, calLines, "UPCOMING", false, 28,
           icon_calendar_data, icon_calendar_width, icon_calendar_height);

  epd_poweroff();
}

void drawDashboard() {
  Serial.println("=== Starting drawDashboard ===");
  epd_poweron();
  epd_clear();

  Serial.println("Screen cleared, drawing content...");

  // ========== TOP HEADER ==========
  // Date (placed at former clock position to reduce refresh frequency)
  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);
  char dateStr[50];
  strftime(dateStr, sizeof(dateStr), "%b %d", &timeinfo); // "Jan 15"
  currentDateString = String(dateStr);

  int32_t date_x = clockArea.x;
  int32_t date_y = clockArea.y + 25; // Use compact positioning
  epd_clear_area(clockArea);
  writeln((GFXfont *)&FiraSansMedium, dateStr, &date_x, &date_y, NULL);

  // WiFi Status Indicator - Draw connected/disconnected icon
  bool wifiConnected = isWiFiConnected();
  Serial.print("WiFi connected: ");
  Serial.println(wifiConnected ? "Yes" : "No");
  Serial.print("WiFi status: ");
  Serial.println(WiFi.status());
  epd_clear_area(wifiStatusArea);
  // Draw WiFi status icon (connected or disconnected)
  drawWiFiStatus(wifiStatusArea.x, wifiStatusArea.y + 2);

  // Weather (Top Center) - Compact temperature display
  updateWeatherSection(false);

  // Date area (was top-right) left blank to avoid extra refresh

  // ========== QUOTE SECTION ==========
  drawQuote();

  // Draw divider below quote section
  // Divider below quote
  drawTextDivider(20, 95, 920);

  // ========== MIDDLE SECTION ==========

  // 4. Todo List (Left Half)
  std::vector<String> todoLines;
  todoLines.reserve(todoList.size() + 1);
  for (const auto &item : todoList) {
    String prefix = item.completed ? "X " : "> ";
    String line = prefix + item.text;
    if (line.length() > 45) { // Reduced for half width
      line = line.substring(0, 42) + "...";
    }
    todoLines.push_back(line);
  }

  if (todoLines.empty()) {
    todoLines.push_back("Nothing due today");
  }

  drawList(todoListArea, todoLines, "TODO", true, 32, icon_todo_data,
           icon_todo_width, icon_todo_height);

  // 5. UPCOMING Calendar Events (Right Half) - Side by side with TODO
  // Format: Date/Time on first line (compact), Title on second line
  Serial.print("=== Building calendar display lines from ");
  Serial.print(calendarEvents.size());
  Serial.println(" events ===");

  std::vector<String> calLines;
  calLines.reserve(calendarEvents.size() * 2 + 1); // two lines per event + fallback
  int eventIndex = 0;
  for (const auto &evt : calendarEvents) {
    Serial.print("Processing event #");
    Serial.print(eventIndex);
    Serial.print(": ");
    Serial.print(evt.title);
    Serial.print(" (");
    Serial.print(evt.isoDateTime);
    Serial.println(")");

    // First line: compact date and time format
    // Convert "01-15" to "1/15" and "14:30" to "2:30 PM" or just "2:30"
    String compactDate = evt.date;
    compactDate.replace("-", "/");
    // Remove leading zeros from month/day
    int dashPos = compactDate.indexOf('/');
    if (dashPos > 0) {
      String month = compactDate.substring(0, dashPos);
      String day = compactDate.substring(dashPos + 1);
      if (month.startsWith("0") && month.length() > 1)
        month = month.substring(1);
      if (day.startsWith("0") && day.length() > 1)
        day = day.substring(1);
      compactDate = month + "/" + day;
    }

    // Format time more compactly - remove leading zeros from hour
    String compactTime = evt.startTime;
    if (compactTime.indexOf(":") > 0) {
      int colonPos = compactTime.indexOf(":");
      String hour = compactTime.substring(0, colonPos);
      String minute = compactTime.substring(colonPos);
      if (hour.startsWith("0") && hour.length() > 1)
        hour = hour.substring(1);
      compactTime = hour + minute;
    }

    String dateTimeLine = compactDate + " " + compactTime;
    calLines.push_back(dateTimeLine);

    // Second line: title (truncate if needed)
    // Add a marker at the start to help identify this as a title (not
    // date/time)
    String titleLine =
        "  " + evt.title; // Indent with 2 spaces to distinguish from date/time
    int maxTitleLength = 28; // Half-width area
    if (titleLine.length() > maxTitleLength) {
      titleLine = titleLine.substring(0, maxTitleLength - 3) + "...";
    }
    calLines.push_back(titleLine);

    Serial.print("  Added line pair - DateTime: '");
    Serial.print(dateTimeLine);
    Serial.print("' Title: '");
    Serial.print(titleLine);
    Serial.println("'");

    eventIndex++;
  }

  Serial.print("Total lines to display: ");
  Serial.println(calLines.size());
  Serial.print("Calendar list area: x=");
  Serial.print(calendarListArea.x);
  Serial.print(" y=");
  Serial.print(calendarListArea.y);
  Serial.print(" width=");
  Serial.print(calendarListArea.width);
  Serial.print(" height=");
  Serial.println(calendarListArea.height);

  if (calLines.empty()) {
    calLines.push_back("No upcoming events");
  }

  // Use consistent line spacing (32) for calendar (2 lines per event: date/time
  // + title)
  Serial.println(">>> Calling drawList for UPCOMING calendar");
  drawList(calendarListArea, calLines, "UPCOMING", false, 32,
           icon_calendar_data, icon_calendar_width, icon_calendar_height);
  Serial.println(">>> drawList for UPCOMING calendar completed");

  // Divider above mini calendar
  drawTextDivider(20, 435, 920);

  // ========== BOTTOM SECTION ==========
  // 6. Mini Calendar (Compact, Bottom Full Width)
  epd_clear_area(miniCalendarArea);
  drawMiniCalendar(miniCalendarArea.x, miniCalendarArea.y,
                   miniCalendarArea.width, miniCalendarArea.height);

  Serial.println("=== drawDashboard complete ===");

  // Power off display to save battery
  // Note: This only powers off the e-paper display, not the ESP32
  // The ESP32 continues running and can be powered by battery or USB-C
  // When USB-C is connected, the board automatically uses USB power and charges
  // the battery
  epd_poweroff();
}

/**
 * Draw initial screen (blank white screen)
 * Called once during setup before connecting to WiFi
 * Works on both battery and USB-C power (automatic switching)
 */
void drawInitialScreen() {
  epd_poweron();
  epd_clear();
  epd_poweroff();
}

/**
 * Update date tracking (no drawing). Called in setup to initialize currentDateString
 * and lastMidnightDay.
 */
void updateDate() {
  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);
  char dateStr[50];
  strftime(dateStr, sizeof(dateStr), "%b %d",
           &timeinfo); // Shorter format: "Jan 15"
  String newDateString = String(dateStr);

  currentDateString = newDateString;
  lastMidnightDay = timeinfo.tm_mday;
  Serial.print("Date initialized to ");
  Serial.println(currentDateString);
}

// ---------- Arduino lifecycle ----------
/**
 * Setup function - Initializes display, WiFi, NTP, and fetches initial data
 *
 * Power Management:
 * - Works on battery power (automatic power switching)
 * - Works while charging via USB-C (board auto-switches to USB power)
 * - Display is powered on only when updating (saves battery)
 * - No manual power source switching needed - handled automatically by hardware
 */
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\nEPD47 Home Assistant Dashboard");

  pinMode(BUTTON_1, INPUT_PULLUP); // Button to open OTA window on demand
  lastButtonState = digitalRead(BUTTON_1);

  // Decide mode at boot based on BUTTON_1 (held = debug/OTA mode, released = battery-optimized mode)
  bool bootButtonPressed = (lastButtonState == LOW);
  if (bootButtonPressed) {
    debugMode = true;
    Serial.println("Debug mode enabled (BUTTON_1 held at boot)");
  } else {
    debugMode = false;
    Serial.println("Battery-optimized mode (BUTTON_1 not held at boot)");
  }

  if (String(OTA_PASSWORD) == "CHANGE_ME_OTA_PASSWORD") {
    Serial.println("FATAL: OTA password is not set. Update OTA_PASSWORD_VALUE in secrets.h and OTA_PASSWORD in .platformio_env.");
    while (true) {
      delay(1000);
    }
  }
  if (String(OTA_PASSWORD).length() < 8) {
    Serial.println("⚠ WARNING: OTA password is shorter than 8 characters. Consider using a stronger password.");
  }

  // Configure ADC for battery reading (ESP32-S3)
  // BATT_PIN is GPIO 14 for ESP32-S3
  // Note: ADC1 is used for GPIO 0-21 on ESP32-S3
  pinMode(BATT_PIN, INPUT);
  analogReadResolution(12);       // 12-bit resolution (0-4095)
  analogSetAttenuation(ADC_11db); // 11dB attenuation allows 0-3.3V range

  // Calibrate ADC reference voltage from eFuse (more accurate than hardcoded
  // value) This is the same method used in the demo example
  esp_adc_cal_characteristics_t adc_chars;
  esp_adc_cal_value_t val_type = esp_adc_cal_characterize(
      ADC_UNIT_1,       // ESP32-S3 battery pin uses ADC1
      ADC_ATTEN_DB_11,  // 11dB attenuation
      ADC_WIDTH_BIT_12, // 12-bit width
      1100,             // Default vref (will be overridden if eFuse available)
      &adc_chars);

  if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
    Serial.print("ADC Calibration: Using eFuse Vref: ");
    Serial.print(adc_chars.vref);
    Serial.println("mV");
    vref = adc_chars.vref;
  } else {
    Serial.print("ADC Calibration: Using default Vref: ");
    Serial.print(vref);
    Serial.println("mV (eFuse not available)");
  }
  Serial.println("================================");

  // Initialize e-paper display
  epd_init();
  drawInitialScreen();

  // Initialize STL collections for display data
  initCollections();

  // Connect to WiFi
  connectWiFi();

  // Setup OTA (Over-The-Air) updates
  // IMPORTANT: If you change the password below, also update .platformio_env
  // file The .platformio_env file contains OTA_IP and OTA_PASSWORD for
  // PlatformIO uploads See extra_scripts/load_env.py and
  // .platformio_env.example for details
  ArduinoOTA.setHostname(
      "epd47-dashboard"); // Hostname for OTA (appears in network)
  ArduinoOTA.setPassword(
      OTA_PASSWORD); // OTA password - keep in secrets.h (matches .platformio_env)
  if (String(OTA_PASSWORD) == "CHANGE_ME_OTA_PASSWORD") {
    Serial.println("⚠ WARNING: OTA password is still the placeholder. Update OTA_PASSWORD_VALUE in secrets.h and OTA_PASSWORD in .platformio_env.");
  }

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }
    Serial.println("Start updating " + type);
    // Turn off display during update
    epd_poweroff_all();
  });

  ArduinoOTA.onEnd([]() { Serial.println("\nEnd"); });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });

  ArduinoOTA.begin();
  Serial.println("OTA ready");
  enableOtaWindow(); // Allow OTA immediately after boot for convenience

  // Init NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // Wait for NTP sync and initialize date
  delay(2000);  // Give NTP time to sync
  updateDate(); // Initialize date display and tracking

  // Initial Fetch and full dashboard draw
  fetchWeather();
  fetchTodos();
  fetchCalendar();
  fetchQuotes();

  drawDashboard();

  // Initialize update timers after initial draw
  unsigned long now = millis();
  lastWeatherUpdate   = now;
  lastCalTodoUpdate   = now;
  lastQuoteFetch      = now;
  lastQuoteRotation   = now;
  lastBatteryUpdate   = now;

  // Read battery once at the end of setup so we have up-to-date info
  Serial.println("=== Battery-optimized mode: finalizing setup cycle ===");
  batteryInfo = readBattery();
  Serial.print("Battery: ");
  Serial.print(batteryInfo.voltage, 3);
  Serial.print("V (");
  Serial.print(batteryInfo.percentage);
  Serial.println("%)");

  if (!debugMode) {
    Serial.println("Battery mode active - entering deep sleep for 15 minutes.");
    // Turn off WiFi before entering deep sleep
    forceWifiOff();
    // Configure wake-up timer: 15 minutes (15 * 60 * 1,000,000 microseconds)
    esp_sleep_enable_timer_wakeup(15ULL * 60ULL * 1000000ULL);
    Serial.println("Entering deep sleep now...");
    esp_deep_sleep_start();
  } else {
    Serial.println("Debug/OTA mode active - staying awake and running loop().");
  }
}

/**
 * Main loop - Handles periodic updates and OTA
 * Runs continuously, updating different sections at their configured intervals
 */
// NOTE: In normal battery-optimized mode, the device enters deep sleep at the end of setup()
// and loop() is never executed. loop() is only used when debugMode is true (BUTTON_1 held at boot)
// to allow continuous OTA and live debugging.
void loop() {
  if (!debugMode) {
    // In battery mode we should be in deep sleep; this is a safety guard.
    return;
  }
  // Handle OTA updates only when OTA window is active
  if (otaEnabled) {
    ArduinoOTA.handle();
    if (millis() > otaWindowEnds) {
      Serial.println("OTA window expired");
      otaEnabled = false;
    } else {
      // While OTA is active, skip the rest of the loop to avoid
      // network activity or e-paper operations interfering with the upload.
      return;
    }
  }

  // Simple button poll to re-enable OTA (active low, edge detected)
  int buttonState = digitalRead(BUTTON_1);
  if (buttonState == LOW && lastButtonState == HIGH) {
    Serial.println("OTA button pressed - enabling OTA window");
    enableOtaWindow();
    connectWiFi();
  }
  lastButtonState = buttonState;

  // If OTA is enabled but WiFi dropped, reconnect
  if (otaEnabled && WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  // If OTA is not active, ensure WiFi is off (LED off)
  if (!otaEnabled) {
    disableWiFiIfAllowed();
  }

  unsigned long now = millis();
  bool lowBatteryMode =
      (!batteryInfo.isCharging && batteryInfo.percentage > 0 &&
       batteryInfo.percentage < 20);
  unsigned long weatherInterval =
      WEATHER_UPDATE_INTERVAL_MS * (lowBatteryMode ? 2 : 1);
  unsigned long calTodoInterval =
      CAL_TODO_UPDATE_INTERVAL_MS * (lowBatteryMode ? 2 : 1);
  unsigned long quoteFetchInterval =
      QUOTE_FETCH_INTERVAL_MS * (lowBatteryMode ? 2 : 1);
  unsigned long quoteRotateInterval =
      QUOTE_ROTATION_INTERVAL_MS * (lowBatteryMode ? 2 : 1);

  // Weather update every hour
  if (lastWeatherUpdate == 0 ||
      (now - lastWeatherUpdate) > weatherInterval) {
    lastWeatherUpdate = now;

    Serial.println("Updating Weather...");
    fetchWeather();
    updateWeatherSection();
  }

  // Calendar and Todo update every 6 hours
  if (lastCalTodoUpdate == 0 ||
      (now - lastCalTodoUpdate) > calTodoInterval) {
    lastCalTodoUpdate = now;

    Serial.println("Updating Calendar and Todo...");
    fetchTodos();
    fetchCalendar();
    updateCalendarTodoSections();
  }

  // Clock removed to save refreshes

  // Quote fetch update (once per day)
  if (lastQuoteFetch == 0 || (now - lastQuoteFetch) > quoteFetchInterval) {
    lastQuoteFetch = now;
    Serial.println("Fetching new quotes...");
    fetchQuotes();
    // Redraw full dashboard to show first quote
    drawDashboard();
  }

  // Quote rotation update (every 6 hours)
  if (lastQuoteRotation == 0 ||
      (now - lastQuoteRotation) > quoteRotateInterval) {
    lastQuoteRotation = now;
    Serial.println("Rotating quote...");
    rotateQuote(); // rotateQuote() handles drawing and power management
  }

  // Midnight full refresh (check once per minute)
  if (lastMidnightCheck == 0 || (now - lastMidnightCheck) > MIDNIGHT_CHECK_INTERVAL_MS) {
    lastMidnightCheck = now;
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      if (timeinfo.tm_hour == 0 && timeinfo.tm_min == 0 &&
          timeinfo.tm_mday != lastMidnightDay) {
        Serial.println("Midnight detected, scheduling full refresh");
        lastMidnightDay = timeinfo.tm_mday;
        fullRefreshScheduled = true;
      }
    }
  }

  // Run scheduled full refresh (e.g., at midnight)
  if (fullRefreshScheduled) {
    fullRefreshScheduled = false;
    drawDashboard();
  }

  // Battery display removed to avoid frequent refresh; still measure for logging
  if (lastBatteryUpdate == 0 ||
      (now - lastBatteryUpdate) > BATTERY_UPDATE_INTERVAL_MS) {
    lastBatteryUpdate = now;

    Serial.println("=== Battery Reading (no display update) ===");
    epd_poweron();
    delay(10);
    batteryInfo = readBattery();
    Serial.print("Battery: ");
    Serial.print(batteryInfo.voltage, 3);
    Serial.print("V (");
    Serial.print(batteryInfo.percentage);
    Serial.println("%)");
    epd_poweroff();
  }

  delay(1000);
}
