/**
 * Simple Home Assistant calendar display
 * Based on LilyGo EPD47 examples, but stripped down:
 * - No web server
 * - No file upload
 * - Just fetches two HA sensors and prints them.
 */

#ifndef BOARD_HAS_PSRAM
#error "Please enable PSRAM, Arduino IDE -> Tools -> PSRAM -> OPI PSRAM"
#endif

#include "epd_driver.h"
#include "firasans.h"
#include "firasans_medium.h"
#include "firasans_small.h"
#include "utilities.h"
#include "secrets.h"
#include "weather_icons.h"
#include "todo_icons.h"
#include "calendar_icons.h"
#include "wifi_icons.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <algorithm>
#include <vector>
#include <cstring>

// ---------- CONFIG ----------
// WiFi (loaded from secrets.h, which is not committed to git)
const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;

// Home Assistant (host/port loaded from secrets.h)
const char *HA_HOST = HA_HOST_ADDR;
const uint16_t HA_PORT = HA_PORT_NUM;

// Long-lived access token from HA (from secrets.h)
const char *HA_TOKEN = HA_TOKEN_VALUE;

// HA entities
// TODO: Move these to secrets.h or update them here
const char *ENTITY_WEATHER =
    "weather.toronto_forecast"; // Replace with your weather entity
const char *ENTITY_QUOTE = "sensor.quote_of_the_day"; // Quote of the day sensor

// NTP Config
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -18000;   // Example: EST (UTC-5). Adjust as needed.
const int daylightOffset_sec = 3600; // 1 hour for DST

// List of todo entities to aggregate (Native HA entities)
const std::vector<const char *> ENTITY_TODOS = {"todo.errands",
                                                "todo.work", 
                                                "todo.personal"};
// List of calendar entities to aggregate
const std::vector<const char *> ENTITY_CALENDARS = {
    "calendar.aman_outlook_calendar", "calendar.home_2"};

// Update intervals (ms)
const unsigned long WEATHER_UPDATE_INTERVAL_MS = 60UL * 60UL * 1000UL; // 1 hour
const unsigned long CAL_TODO_UPDATE_INTERVAL_MS = 60UL * 60UL * 1000UL; // 1 hour
const unsigned long CLOCK_UPDATE_INTERVAL_MS = 60UL * 1000UL;      // 1 minute
const unsigned long DATE_UPDATE_INTERVAL_MS = 24UL * 60UL * 60UL * 1000UL; // 24 hours (once per day)
const unsigned long QUOTE_ROTATION_INTERVAL_MS = 3UL * 60UL * 60UL * 1000UL; // 3 hours
const unsigned long QUOTE_FETCH_INTERVAL_MS = 24UL * 60UL * 60UL * 1000UL; // 24 hours (fetch new quotes once per day)

// ---------- Data Structures ----------
struct WeatherData {
  String temperature;
  String condition;
  String icon; // e.g., "mdi:weather-sunny" (we might just use condition text
               // for now)
};

struct TodoItem {
  String text;
  bool completed;
};

struct CalendarEvent {
  String title;
  String startTime; // formatted string (HH:MM)
  String date;      // formatted string (MM-DD)
  String isoDateTime; // Original ISO datetime for countdown calculation
};

struct QuoteData {
  String author;
  String text;
};

// Global Data
WeatherData currentWeather;
std::vector<TodoItem> todoList;
std::vector<CalendarEvent> calendarEvents;
std::vector<QuoteData> quotes;
int currentQuoteIndex = 0;

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

// Top Header - Time, Weather, and Date (Screen: 960x540)
const Rect_t clockArea = {.x = 20, .y = 20, .width = 120, .height = 35};  // Clock in top left, very compact
const Rect_t weatherArea = {.x = 420, .y = 20, .width = 120, .height = 35};  // Weather in top center, same size
const Rect_t dateArea = {.x = 820, .y = 20, .width = 120, .height = 35};  // Date in top right, same size as clock
const Rect_t wifiStatusArea = {.x = 150, .y = 20, .width = 60, .height = 35};  // WiFi status next to clock

// Quote Section - Between header and todo (full width, single line)
const Rect_t quoteArea = {.x = 20, .y = 65, .width = 920, .height = 40};  // Full width for daily quote (quote only, no author)

// Middle Section - Todo (left half) and UPCOMING Calendar (right half) side by side
// Moved up since quote section is now smaller (no author)
// Calculating from bottom: Screen is 540px tall
// Mini calendar: 50px at bottom (y=490 to y=540)
// Divider needs ~15px space (y=475)
// TODO/UPCOMING must end before divider, so max y=470
// TODO/UPCOMING starts at y=160, so max height = 470 - 160 = 310px
const Rect_t todoHeaderArea = {.x = 20, .y = 115, .width = 450, .height = 40};  // Left half (moved up)
const Rect_t todoListArea = {.x = 20, .y = 160, .width = 450, .height = 280};  // Left half - calculated to end at y=440, leaving 30px buffer before divider
const Rect_t calendarHeaderArea = {.x = 490, .y = 115, .width = 450, .height = 40};  // Right half (moved up)
const Rect_t calendarListArea = {.x = 490, .y = 160, .width = 450, .height = 280};  // Right half - calculated to end at y=440, leaving 30px buffer before divider

// Bottom Section - Mini Calendar (compact, full width)
// Screen is 960x540, so we need to ensure it fits within bounds
// Need enough height for 2 rows: day labels (~30px) + spacing + day numbers (~30px)
// Positioned at bottom: y=490 to y=540 (50px height)
const Rect_t miniCalendarArea = {.x = 20, .y = 490, .width = 920, .height = 45};  // Compact mini calendar at bottom, moved up slightly

// ---------- Globals ----------
unsigned long lastWeatherUpdate = 0;
unsigned long lastCalTodoUpdate = 0;
unsigned long lastClockUpdate = 0;
unsigned long lastDateUpdate = 0;
unsigned long lastQuoteFetch = 0;
unsigned long lastQuoteRotation = 0;
String currentDateString = ""; // Track current date to detect day changes

// Last drawn text for change detection
String lastClockText;
String lastCalLine1Text;
String lastCalLine2Text;

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
  } else {
    Serial.println("WiFi connect failed");
  }
}

// ---------- HA REST helper ----------
// Generic helper to fetch JSON from HA
// Returns true if successful, false otherwise. Populates doc.
bool fetchJson(const String &url, JsonDocument &doc) {
  Serial.print("fetchJson: Connecting to WiFi...");
  connectWiFi();
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(" FAILED - WiFi not connected");
    return false;
  }
  Serial.println(" OK");

  Serial.print("fetchJson: Starting HTTP request to: ");
  Serial.println(url);
  
  HTTPClient http;
  http.setTimeout(10000); // 10 second timeout
  http.begin(url);
  http.addHeader("Authorization", String("Bearer ") + HA_TOKEN);
  http.addHeader("Content-Type", "application/json");

  int httpCode = http.GET();
  Serial.print("fetchJson: HTTP response code: ");
  Serial.println(httpCode);
  
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("ERROR: HTTP Error: %d\n", httpCode);
    String errorPayload = http.getString();
    Serial.print("Error response: ");
    if (errorPayload.length() > 0) {
      Serial.println(errorPayload);
    } else {
      Serial.println("(empty response)");
    }
    http.end();
    return false;
  }

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

void fetchWeather() {
  Serial.println("=== fetchWeather() called ===");
  String url = String("http://") + HA_HOST + ":" + HA_PORT + "/api/states/" +
               ENTITY_WEATHER;
  Serial.print("Weather URL: ");
  Serial.println(url);
  
  JsonDocument doc;

  if (!fetchJson(url, doc)) {
    Serial.println("ERROR: Weather fetch failed - fetchJson returned false");
    return;
  }

  Serial.println("Weather JSON fetched successfully");
  
  const char *state = doc["state"];
  
  // Check if temperature exists and is valid
  if (!doc["attributes"]["temperature"].is<float>()) {
    Serial.println("WARNING: Temperature not found or invalid in JSON");
    currentWeather.temperature = "-- C";
  } else {
    float temp = doc["attributes"]["temperature"];
    currentWeather.temperature = String(temp, 1) + " C";
  }

  currentWeather.condition = state ? String(state) : "--";
  currentWeather.icon = "";   // TODO: Map state to icon
  
  Serial.print("Weather condition: ");
  Serial.println(currentWeather.condition);
  Serial.print("Weather temperature: ");
  Serial.println(currentWeather.temperature);
  Serial.println("=== fetchWeather() complete ===");
}

void fetchQuotes() {
  Serial.println("=== fetchQuotes() called ===");
  String url = String("http://") + HA_HOST + ":" + HA_PORT + "/api/states/" +
               ENTITY_QUOTE;
  Serial.print("Quote URL: ");
  Serial.println(url);
  
  JsonDocument doc;

  if (!fetchJson(url, doc)) {
    Serial.println("ERROR: Quote fetch failed - fetchJson returned false");
    return;
  }

  Serial.println("Quote JSON fetched successfully");
  
  // Clear existing quotes
  quotes.clear();
  
  // Parse the entries array from attributes
  JsonArray entries = doc["attributes"]["entries"];
  if (!entries) {
    Serial.println("ERROR: No 'entries' array found in attributes");
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
bool fetchJsonPost(const String &url, const String &payload,
                   JsonDocument &doc) {
  Serial.print("fetchJsonPost: Connecting to WiFi...");
  connectWiFi();
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("FAILED - WiFi not connected");
    return false;
  }
  Serial.println("OK");

  Serial.print("fetchJsonPost: URL: ");
  Serial.println(url);
  Serial.print("fetchJsonPost: Payload: ");
  Serial.println(payload);

  HTTPClient http;
  http.setTimeout(10000); // 10 second timeout
  http.begin(url);
  http.addHeader("Authorization", String("Bearer ") + HA_TOKEN);
  http.addHeader("Content-Type", "application/json");
  
  int httpCode = http.POST(payload);
  Serial.print("fetchJsonPost: HTTP response code: ");
  Serial.println(httpCode);
  
  String response = http.getString(); // Get response regardless of code for debug

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

  todoList.clear();
  JsonDocument doc; // Larger buffer for service response

  for (const char *entity : ENTITY_TODOS) {
    String url = String("http://") + HA_HOST + ":" + HA_PORT +
                 "/api/services/todo/get_items?return_response=true";
    // Add status: needs_action to be explicit and match common usage
    String payload = String("{\"entity_id\": \"") + entity +
                     "\", \"status\": \"needs_action\"}";

    doc.clear();

    if (!fetchJsonPost(url, payload, doc))
      continue;

    // Service response structure (with return_response=true):
    // {
    //   "service_response": {
    //     "todo.errands": {
    //       "items": [ ... ]
    //     }
    //   }
    // }

    JsonArray items = doc["service_response"][entity]["items"];
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
        todoList.push_back(item);
      }
    }
  }

  // Limit to 6 items
  if (todoList.size() > 6) {
    todoList.resize(6);
  }
}

// Helper to get URL-encoded ISO8601 string for Calendar API
String getISOTime(time_t t) {
  struct tm *tm = localtime(&t);
  char buf[30];
  // Format: YYYY-MM-DDTHH:MM:SS
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", tm);
  return String(buf);
}

void fetchCalendar() {
  Serial.println("=== fetchCalendar() called ===");
  calendarEvents.clear();
  JsonDocument doc;

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
    String url = String("http://") + HA_HOST + ":" + HA_PORT +
                 "/api/calendars/" + entity + "?start=" + startStr +
                 "&end=" + endStr;

    Serial.print("Fetching Calendar URL: ");
    Serial.println(url);
    doc.clear();

    if (!fetchJson(url, doc)) {
      Serial.print("ERROR: Failed to fetch calendar: ");
      Serial.println(entity);
      Serial.print("URL was: ");
      Serial.println(url);
      Serial.println(">>> Moving to next calendar entity");
      continue;
    }
    
    Serial.println(">>> fetchJson succeeded");
    
    // Check if we got valid calendar data
    if (!doc.is<JsonArray>()) {
      Serial.print("WARNING: Calendar response was not an array for entity: ");
      Serial.println(entity);
      Serial.print("Response type: ");
      if (doc.is<JsonObject>()) {
        Serial.println("Object (unexpected)");
        serializeJson(doc, Serial);
        Serial.println();
      } else {
        Serial.println("Unknown");
      }
      continue;
    }

    Serial.print("Calendar JSON fetched successfully for: ");
    Serial.println(entity);

    // The API returns a JSON Array of events directly
    if (doc.is<JsonArray>()) {
      JsonArray events = doc.as<JsonArray>();
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
        calendarEvents.push_back(evt);
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
      if (doc.is<JsonObject>()) {
        Serial.println("Object");
        serializeJson(doc, Serial);
        Serial.println();
      } else {
        Serial.println("Unknown");
      }
    }
  }
  
  Serial.print("Total calendar events fetched: ");
  Serial.println(totalEvents);

  // Format for display BEFORE sorting and limiting
  for (auto &evt : calendarEvents) {
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
  std::sort(calendarEvents.begin(), calendarEvents.end(),
            [](const CalendarEvent &a, const CalendarEvent &b) {
              return a.isoDateTime < b.isoDateTime;
            });
  
  // NOW limit to display area (increase to 10 items to show more events for the week)
  // Each event takes 2 lines (date/time + title), so 10 items = 5 events
  if (calendarEvents.size() > 10) {
    calendarEvents.resize(10);
  }

  Serial.print("Calendar events after sorting and limiting: ");
  Serial.println(calendarEvents.size());
  
  Serial.print("Final calendar events count after formatting: ");
  Serial.println(calendarEvents.size());
  Serial.println("=== fetchCalendar() complete ===");
}

// Helper to get current time string from NTP
String fetchTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "--:--";
  }
  char timeStringBuff[10];
  strftime(timeStringBuff, sizeof(timeStringBuff), "%I:%M", &timeinfo);  // 12-hour format (01-12)
  return String(timeStringBuff);
}

// ---------- Icon Drawing Functions ----------
// Draw bitmap icons using pre-defined data
void drawBitmapIcon(int32_t x, int32_t y, const uint8_t *icon_data, uint32_t width, uint32_t height) {
  Rect_t icon_area = {.x = x, .y = y, .width = (int32_t)width, .height = (int32_t)height};
  epd_draw_image(icon_area, (uint8_t *)icon_data, BLACK_ON_WHITE);
}

// ---------- Drawing helpers ----------
// Draw a text-based divider line using dashes
void drawTextDivider(int32_t x, int32_t y, int32_t width) {
  // Create a string of dashes (approximately 1 dash per 10 pixels for visibility)
  int32_t numDashes = width / 10;
  if (numDashes < 1) numDashes = 1;
  
  String dividerText = "";
  for (int32_t i = 0; i < numDashes; i++) {
    dividerText += "-";
  }
  
  // Draw the divider text
  int32_t cursor_x = x;
  int32_t cursor_y = y + FiraSans.advance_y + FiraSans.descender;
  writeln((GFXfont *)&FiraSans, dividerText.c_str(), &cursor_x, &cursor_y, NULL);
}

// Draw daily motivation quote
void drawQuote() {
  if (quotes.empty()) {
    Serial.println("No quotes available");
    return;
  }
  
  // Get current quote
  QuoteData currentQuote = quotes[currentQuoteIndex];
  
  // Format quote text with quotes (no author)
  String quoteText = "\"" + currentQuote.text + "\"";
  
  // Truncate quote if too long - quoteArea is 920px wide, FiraSansMedium is ~8-10px per char
  // So max ~90-100 characters. Use 90 to be safe and leave margin
  if (quoteText.length() > 90) {
    quoteText = quoteText.substring(0, 87) + "...";
  }
  
  Serial.print("Drawing quote: ");
  Serial.println(quoteText);
  
  // Clear and draw quote (single line, no author)
  // Clear the quote area (don't extend beyond screen bounds - screen is 960px wide)
  epd_clear_area(quoteArea);
  int32_t cursor_x = quoteArea.x;
  int32_t cursor_y = quoteArea.y + FiraSansMedium.advance_y + FiraSansMedium.descender;
  
  // Draw quote text only
  writeln((GFXfont *)&FiraSansMedium, quoteText.c_str(), &cursor_x, &cursor_y, NULL);
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

// Check if WiFi is connected
bool isWiFiConnected() {
  return (WiFi.status() == WL_CONNECTED);
}

// Draw WiFi status icon - connected or disconnected
void drawWiFiStatus(int32_t x, int32_t y) {
  if (isWiFiConnected()) {
    drawBitmapIcon(x, y, icon_wifi_connected_data, icon_wifi_connected_width, icon_wifi_connected_height);
  } else {
    drawBitmapIcon(x, y, icon_wifi_disconnected_data, icon_wifi_disconnected_width, icon_wifi_disconnected_height);
  }
}

// Parse ISO datetime string to time_t
time_t parseISODateTime(const String &isoStr) {
  if (isoStr.length() < 10) return 0;
  
  struct tm timeinfo = {0};
  // Parse YYYY-MM-DDTHH:MM:SS or YYYY-MM-DD
  int year, month, day, hour = 0, minute = 0, second = 0;
  
  if (sscanf(isoStr.c_str(), "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &minute, &second) >= 3 ||
      sscanf(isoStr.c_str(), "%d-%d-%d", &year, &month, &day) >= 3) {
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
  if (calendarEvents.empty()) return -1;
  
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

// Get task completion stats
void getTaskStats(int &completed, int &total) {
  completed = 0;
  total = todoList.size();
  for (const auto &item : todoList) {
    if (item.completed) completed++;
  }
}

// Draw a progress bar using a small framebuffer
void drawProgressBar(int32_t x, int32_t y, int32_t width, int32_t height, float progress) {
  // Clamp progress between 0 and 1
  if (progress < 0) progress = 0;
  if (progress > 1) progress = 1;
  
  // Ensure minimum size
  if (width < 4 || height < 4) {
    Serial.println("Progress bar: size too small");
    return;
  }
  
  // Limit size to prevent memory issues
  if (width > 200 || height > 20) {
    Serial.println("Progress bar: size too large");
    return;
  }
  
  // Allocate small framebuffer for progress bar
  size_t bufferSize = (width * height + 1) / 2; // Round up for odd pixel counts
  uint8_t *barBuffer = (uint8_t *)ps_calloc(sizeof(uint8_t), bufferSize);
  if (!barBuffer) {
    Serial.print("Progress bar: allocation failed, size=");
    Serial.println(bufferSize);
    return; // Skip if allocation fails
  }
  
  // Clear to white
  memset(barBuffer, 0xFF, bufferSize);
  
  // Draw border (coordinates relative to framebuffer, starting at 0,0)
  Rect_t barArea = {.x = x, .y = y, .width = width, .height = height};
  epd_draw_rect(0, 0, width, height, 0, barBuffer);
  
  // Draw filled portion
  int32_t fillWidth = (int32_t)(width * progress);
  if (fillWidth > 2) {
    epd_fill_rect(1, 1, fillWidth - 2, height - 2, 0, barBuffer);
  }
  
  // Draw the framebuffer to screen at position (x, y)
  epd_draw_grayscale_image(barArea, barBuffer);
  
  // Free framebuffer immediately
  free(barBuffer);
  barBuffer = NULL;
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
  if (currentMonth == 4 || currentMonth == 6 || currentMonth == 9 || currentMonth == 11) {
    daysInMonth = 30;
  } else if (currentMonth == 2) {
    daysInMonth = ((currentYear % 4 == 0 && currentYear % 100 != 0) || (currentYear % 400 == 0)) ? 29 : 28;
  }
  
  // Adjust if startDay is before month start
  if (startDay < 1) {
    startDay = 1;
  }
  
  // Compact layout: day labels and numbers on same line, using small font
  const char* dayLabels = "SMTWTFS";
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
  cursor_y = y + FiraSansSmall.advance_y + 18; // 18px gap between labels and numbers for better readability
  
  for (int i = 0; i < 7; i++) {
    int day = startDay + i;
    if (day > daysInMonth) break; // Past end of month
    
    int32_t cursor_x = x + (i * dayWidth) + (dayWidth / 2) - 3; // Center day number
    
    // Highlight today with a box border
    bool isToday = (day == currentDay);
    
    // Draw day number first
    char dayStr[4];
    snprintf(dayStr, sizeof(dayStr), "%d", day);
    writeln((GFXfont *)&FiraSansSmall, dayStr, &cursor_x, &cursor_y, NULL);
    
    if (isToday) {
      // Draw a box border around today's date
      // Calculate box position - center it around the day number
      int32_t boxX = x + (i * dayWidth) + 3;
      int32_t boxY = cursor_y - FiraSansSmall.advance_y - 3;
      int32_t boxWidth = dayWidth - 6;
      int32_t boxHeight = FiraSansSmall.advance_y + 4;
      
      // Use a small framebuffer to draw the border
      size_t bufferSize = (boxWidth * boxHeight + 1) / 2;
      if (bufferSize > 0 && bufferSize < 1000) { // Safety check
        uint8_t *boxBuffer = (uint8_t *)ps_calloc(sizeof(uint8_t), bufferSize);
        if (boxBuffer) {
          memset(boxBuffer, 0xFF, bufferSize); // Clear to white
          // Draw rectangle border (black border on white background)
          epd_draw_rect(0, 0, boxWidth, boxHeight, 0, boxBuffer);
          // Draw the framebuffer to screen
          Rect_t boxArea = {.x = boxX, .y = boxY, .width = boxWidth, .height = boxHeight};
          epd_draw_grayscale_image(boxArea, boxBuffer);
          free(boxBuffer);
        }
      }
    }
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
  writeln((GFXfont *)&FiraSans, displayText.c_str(), &cursor_x, &cursor_y, NULL);
}

void drawList(const Rect_t &area, const std::vector<String> &lines,
              String header, bool drawIcons = false, int lineSpacing = 35,
              const uint8_t *headerIconData = NULL, uint32_t headerIconWidth = 0, uint32_t headerIconHeight = 0) {
  epd_clear_area(area);

  int32_t cursor_x = area.x;
  int32_t cursor_y = area.y + FiraSans.advance_y + FiraSans.descender;

  // Draw Header (only if provided and non-empty)
  if (header.length() > 0) {
    // Draw header icon if provided
    if (headerIconData != NULL && headerIconWidth > 0 && headerIconHeight > 0) {
      int32_t icon_x = cursor_x;
      int32_t icon_y = cursor_y - headerIconHeight - 2; // Position icon slightly above text baseline
      drawBitmapIcon(icon_x, icon_y, headerIconData, headerIconWidth, headerIconHeight);
      cursor_x += headerIconWidth + 5; // Space after icon
    }
    
    writeln((GFXfont *)&FiraSansMedium, header.c_str(), &cursor_x, &cursor_y, NULL);
    
    // If this is UPCOMING header, add countdown next to it
    if (header == "UPCOMING") {
      cursor_x += 10; // Space between header and countdown
      int minutesUntilNext = getMinutesUntilNextEvent();
      if (minutesUntilNext >= 0) {
        int hours = minutesUntilNext / 60;
        int mins = minutesUntilNext % 60;
        char countdownStr[20];
        if (hours > 0) {
          snprintf(countdownStr, sizeof(countdownStr), "(Next: %dh %dm)", hours, mins);
        } else {
          snprintf(countdownStr, sizeof(countdownStr), "(Next: %dm)", mins);
        }
        writeln((GFXfont *)&FiraSansSmall, countdownStr, &cursor_x, &cursor_y, NULL);
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
        drawBitmapIcon(icon_x, icon_y, icon_checkbox_checked_data, icon_checkbox_checked_width, icon_checkbox_checked_height);
      } else {
        drawBitmapIcon(icon_x, icon_y, icon_checkbox_data, icon_checkbox_width, icon_checkbox_height);
      }
      
      cursor_x += icon_checkbox_width + 5; // Space after icon
    }
    
    // Truncate long lines to fit in area width
    String displayLine = line;
    
    // Store original line for detection (before any modifications)
    String originalLine = line;
    
    // Remove icon prefix if present
    if (displayLine.length() > 2 && (displayLine[0] == 'X' || displayLine[0] == '>')) {
      displayLine = displayLine.substring(2);
    }
    // Adjust max length based on area width (450px = ~28 chars, 920px = ~50 chars)
    int maxChars = (area.width < 500) ? 28 : 50; // Half width gets 28 chars, full width gets 50
    if (displayLine.length() > maxChars) {
      displayLine = displayLine.substring(0, maxChars - 3) + "...";
    }
    
    // Check if this is a date/time line (starts with date pattern like "1/15" or contains "All Day")
    // Use ORIGINAL line (before truncation) for detection
    bool isDateTimeLine = false;
    
    // More robust check: line must start with a digit and contain "/" within first 6 chars
    // OR contain "All Day"
    // Also check: lines that start with spaces are NOT date/time (they're indented titles)
    if (originalLine.indexOf("All Day") >= 0) {
      isDateTimeLine = true;
    } else if (originalLine.length() > 0) {
      // Check if first character is a digit (month) - NOT a space
      char firstChar = originalLine.charAt(0);
      if (firstChar >= '0' && firstChar <= '9') {
        // Now check if there's a "/" within first 6 characters (covers "1/15", "10/15", "12/1")
        int slashPos = originalLine.indexOf('/');
        if (slashPos >= 1 && slashPos <= 5) {
          isDateTimeLine = true;
        }
      }
    }
    
    // Debug logging for UPCOMING events
    if (!drawIcons && itemIndex < 6) { // Log first 6 items to see date/time pairs
      Serial.print("Line ");
      Serial.print(itemIndex);
      Serial.print(": original='");
      Serial.print(originalLine.substring(0, min(20, (int)originalLine.length())));
      Serial.print("' display='");
      Serial.print(displayLine.substring(0, min(20, (int)displayLine.length())));
      Serial.print("' isDateTime=");
      Serial.println(isDateTimeLine);
    }
    
    // Use smaller font for date/time lines in calendar (not for todo items)
    if (isDateTimeLine && !drawIcons) {
      writeln((GFXfont *)&FiraSansSmall, displayLine.c_str(), &cursor_x, &cursor_y, NULL);
      // Smaller font needs adjustment - add spacing to prevent overlap with title
      cursor_y += lineSpacing + 2; // Add extra spacing after small date/time line
    } else {
      writeln((GFXfont *)&FiraSansMedium, displayLine.c_str(), &cursor_x, &cursor_y, NULL);
      // Normal spacing for all other lines
      cursor_y += lineSpacing;
    }
    
    // Check if next line would overflow (check BEFORE incrementing for next line)
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
  int32_t icon_y = weatherArea.y - 6;  // Slightly above to center better in 35px height
  
  // Use smaller icon size - draw 32x32 from 48x48 icon (centered)
  if (condition.indexOf("sun") >= 0 || condition.indexOf("clear") >= 0) {
    // Draw sun icon - use a smaller area (32x32) from the 48x48 icon
    Rect_t icon_area = {.x = icon_x, .y = icon_y, .width = 32, .height = 32};
    // We'll draw the full icon but it will be clipped to the area
    drawBitmapIcon(icon_x, icon_y, icon_sun_data, icon_sun_width, icon_sun_height);
  } else {
    // Draw cloud icon
    drawBitmapIcon(icon_x, icon_y, icon_cloud_data, icon_cloud_width, icon_cloud_height);
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
  
  // Draw temperature text next to icon (starting after icon + larger gap) - medium font
  int32_t weather_x = weatherArea.x + 50;  // Start after icon (48px) + 2px gap for better spacing
  int32_t weather_y = weatherArea.y + 25;  // Same offset as clock/date for alignment
  writeln((GFXfont *)&FiraSansMedium, tempDisplay.c_str(), &weather_x, &weather_y, NULL);
  
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
  for (const auto &item : todoList) {
    String prefix = item.completed ? "X " : "> ";
    todoLines.push_back(prefix + item.text);
  }
  
  if (todoLines.empty()) {
    todoLines.push_back("X Nothing due today");
  }
  
  drawList(todoListArea, todoLines, "TODO", true, 35, icon_todo_data, icon_todo_width, icon_todo_height);

  // Update Calendar Events
  std::vector<String> calLines;
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
  drawList(calendarListArea, calLines, "UPCOMING", false, 28, icon_calendar_data, icon_calendar_width, icon_calendar_height);
  
  epd_poweroff();
}

void drawDashboard() {
  Serial.println("=== Starting drawDashboard ===");
  epd_poweron();
  epd_clear();
  
  Serial.println("Screen cleared, drawing content...");

  // ========== TOP HEADER ==========
  // 1. Clock (Top Left) - No icon, smaller font
  String timeStr = fetchTime();
  Serial.print("Time: ");
  Serial.println(timeStr);
  
  // Draw time text in top left corner (no icon) - compact size with medium font
  int32_t clock_x = clockArea.x;
  int32_t clock_y = clockArea.y + 25;  // Adjusted for very compact area
  epd_clear_area(clockArea);
  writeln((GFXfont *)&FiraSansMedium, timeStr.c_str(), &clock_x, &clock_y, NULL);

  // WiFi Status Indicator - Draw connected/disconnected icon
  bool wifiConnected = isWiFiConnected();
  Serial.print("WiFi connected: ");
  Serial.println(wifiConnected ? "Yes" : "No");
  Serial.print("WiFi status: ");
  Serial.println(WiFi.status());
  epd_clear_area(wifiStatusArea);
  // Draw WiFi status icon (connected or disconnected)
  drawWiFiStatus(wifiStatusArea.x, wifiStatusArea.y + 2);

  // 2. Weather (Top Center) - Compact temperature display
  updateWeatherSection(false);

  // 3. Date (Top Right) - Updated separately via updateDate() function (once per day)
  // Draw current date (will be updated separately if it changes) - compact size matching clock
  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);
  char dateStr[50];
  strftime(dateStr, sizeof(dateStr), "%b %d", &timeinfo);  // Shorter format: "Jan 15" instead of "Monday, January 15"
  String newDateString = String(dateStr);
  currentDateString = newDateString; // Update tracking variable
  
  int32_t date_x = dateArea.x;
  int32_t date_y = dateArea.y + 25;  // Same offset as clock for compact size
  epd_clear_area(dateArea);
  writeln((GFXfont *)&FiraSansMedium, dateStr, &date_x, &date_y, NULL);
  
  // ========== QUOTE SECTION ==========
  drawQuote();
  
  // Draw divider below quote section
  drawTextDivider(20, 110, 920);
  
  // ========== MIDDLE SECTION ==========
  
  // 4. Todo List (Left Half)
  std::vector<String> todoLines;
  for (const auto &item : todoList) {
    String prefix = item.completed ? "X " : "> ";
    String line = prefix + item.text;
    if (line.length() > 45) {  // Reduced for half width
      line = line.substring(0, 42) + "...";
    }
    todoLines.push_back(line);
  }
  
  if (todoLines.empty()) {
    todoLines.push_back("Nothing due today");
  }
  
  drawList(todoListArea, todoLines, "TODO", true, 32, icon_todo_data, icon_todo_width, icon_todo_height);
  
  // 5. UPCOMING Calendar Events (Right Half) - Side by side with TODO
  // Format: Date/Time on first line (compact), Title on second line
  Serial.print("=== Building calendar display lines from ");
  Serial.print(calendarEvents.size());
  Serial.println(" events ===");
  
  std::vector<String> calLines;
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
      if (month.startsWith("0") && month.length() > 1) month = month.substring(1);
      if (day.startsWith("0") && day.length() > 1) day = day.substring(1);
      compactDate = month + "/" + day;
    }
    
    // Format time more compactly - remove leading zeros from hour
    String compactTime = evt.startTime;
    if (compactTime.indexOf(":") > 0) {
      int colonPos = compactTime.indexOf(":");
      String hour = compactTime.substring(0, colonPos);
      String minute = compactTime.substring(colonPos);
      if (hour.startsWith("0") && hour.length() > 1) hour = hour.substring(1);
      compactTime = hour + minute;
    }
    
    String dateTimeLine = compactDate + " " + compactTime;
    calLines.push_back(dateTimeLine);
    
    // Second line: title (truncate if needed)
    // Add a marker at the start to help identify this as a title (not date/time)
    String titleLine = "  " + evt.title; // Indent with 2 spaces to distinguish from date/time
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
  
  // Use consistent line spacing (32) for calendar (2 lines per event: date/time + title)
  Serial.println(">>> Calling drawList for UPCOMING calendar");
  drawList(calendarListArea, calLines, "UPCOMING", false, 32, icon_calendar_data, icon_calendar_width, icon_calendar_height);
  Serial.println(">>> drawList for UPCOMING calendar completed");
  
  // Draw divider below TODO/UPCOMING sections, before mini calendar
  // TODO/UPCOMING areas end at y=440 (160 + 280)
  // Divider text draws at y + advance_y + descender (~y+50px), so if divider at y=440, text draws at ~y=490
  // Mini calendar starts at y=490, so we need divider at y=440 or lower
  // Place divider at y=440 (right after TODO/UPCOMING ends), text will draw at ~y=490
  // Move mini calendar down to y=495 to give 5px gap after divider text
  drawTextDivider(20, 440, 920);
  
  // ========== BOTTOM SECTION ==========
  // 6. Mini Calendar (Compact, Bottom Full Width)
  // Positioned at y=495 to y=545, but screen is only 540px, so clip to y=490 to y=540
  // Actually, let's keep it at y=490 but ensure divider doesn't overlap
  epd_clear_area(miniCalendarArea);
  drawMiniCalendar(miniCalendarArea.x, miniCalendarArea.y, miniCalendarArea.width, miniCalendarArea.height);

  Serial.println("=== drawDashboard complete ===");
  
  epd_poweroff();
}

void drawInitialScreen() {
  epd_poweron();
  epd_clear();
  epd_poweroff();
}

// Update date display - only called when date changes (once per day)
void updateDate() {
  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);
  char dateStr[50];
  strftime(dateStr, sizeof(dateStr), "%b %d", &timeinfo);  // Shorter format: "Jan 15"
  String newDateString = String(dateStr);
  
  // Only update if date has changed
  if (newDateString != currentDateString) {
    Serial.print("Date changed from '");
    Serial.print(currentDateString);
    Serial.print("' to '");
    Serial.print(newDateString);
    Serial.println("' - Updating date display");
    
    currentDateString = newDateString;
    epd_poweron();
    int32_t date_x = dateArea.x;
    int32_t date_y = dateArea.y + 25;  // Same offset as clock for compact size
    epd_clear_area(dateArea);
    writeln((GFXfont *)&FiraSans, dateStr, &date_x, &date_y, NULL);
    epd_poweroff();
    
    lastDateUpdate = millis();
  } else {
    Serial.println("Date unchanged, no update needed");
  }
}

// ---------- Arduino lifecycle ----------
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\nEPD47 Home Assistant Dashboard");

  epd_init();
  drawInitialScreen();

  connectWiFi();

  // Init NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  // Wait for NTP sync and initialize date
  delay(2000); // Give NTP time to sync
  updateDate(); // Initialize date display and tracking

  // Initial Fetch and full dashboard draw
  fetchWeather();
  fetchTodos();
  fetchCalendar();
  fetchQuotes();

  drawDashboard();
  
  // Initialize update timers after initial draw
  unsigned long now = millis();
  lastWeatherUpdate = now;
  lastCalTodoUpdate = now;
  lastClockUpdate = now;
  lastQuoteFetch = now;
  lastQuoteRotation = now;
}

void loop() {
  unsigned long now = millis();

  // Weather update every hour
  if (lastWeatherUpdate == 0 || (now - lastWeatherUpdate) > WEATHER_UPDATE_INTERVAL_MS) {
    lastWeatherUpdate = now;
    
    Serial.println("Updating Weather...");
    fetchWeather();
    updateWeatherSection();
  }

  // Calendar and Todo update every 6 hours
  if (lastCalTodoUpdate == 0 || (now - lastCalTodoUpdate) > CAL_TODO_UPDATE_INTERVAL_MS) {
    lastCalTodoUpdate = now;
    
    Serial.println("Updating Calendar and Todo...");
    fetchTodos();
    fetchCalendar();
    updateCalendarTodoSections();
  }

  // Clock update every minute
  if (lastClockUpdate == 0 ||
      (now - lastClockUpdate) > CLOCK_UPDATE_INTERVAL_MS) {
    lastClockUpdate = now;

    Serial.println("Updating Clock...");
    // Only redraw clock area to save power/time (clock icon is NOT updated)
    // Use same positioning as drawDashboard to prevent text jumping
    epd_poweron();
    String timeStr = fetchTime();
    
    // Clear a slightly larger area to ensure all text (including AM/PM) is removed
    // This prevents ghosting/bold text from multiple draws
    Rect_t clearArea = {
      .x = clockArea.x - 5,  // Extend left
      .y = clockArea.y - 5,  // Extend up
      .width = clockArea.width + 10,  // Extend right
      .height = clockArea.height + 10  // Extend down
    };
    
    // Use more aggressive clearing with multiple cycles
    epd_clear_area_cycles(clearArea, 8, 50);  // 8 cycles for thorough clearing
    delay(100);  // Longer delay to ensure clear completes fully
    
    int32_t clock_x = clockArea.x;
    int32_t clock_y = clockArea.y + 25;  // Same offset as drawDashboard
    writeln((GFXfont *)&FiraSansMedium, timeStr.c_str(), &clock_x, &clock_y, NULL);
    epd_poweroff();
  }

  // Date update check - check periodically (every hour) if date has changed
  // Date only updates once per day when it actually changes
  if (lastDateUpdate == 0 || (now - lastDateUpdate) > WEATHER_UPDATE_INTERVAL_MS) {
    updateDate(); // This function checks if date actually changed before updating
  }

  // Quote fetch update (once per day)
  if (lastQuoteFetch == 0 || (now - lastQuoteFetch) > QUOTE_FETCH_INTERVAL_MS) {
    lastQuoteFetch = now;
    Serial.println("Fetching new quotes...");
    fetchQuotes();
    // Redraw full dashboard to show first quote
    drawDashboard();
  }

  // Quote rotation update (every 3 hours)
  if (lastQuoteRotation == 0 || (now - lastQuoteRotation) > QUOTE_ROTATION_INTERVAL_MS) {
    lastQuoteRotation = now;
    Serial.println("Rotating quote...");
    rotateQuote();
  }

  delay(1000);
}