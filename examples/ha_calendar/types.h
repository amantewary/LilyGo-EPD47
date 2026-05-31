#pragma once

#include <Arduino.h>
#include <vector>

struct WeatherData {
  String temperature;
  String condition;
};

struct TodoItem {
  String text;
  String dueDate;  // YYYY-MM-DD
  bool overdue;
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

struct BatteryData {
  float voltage;
  int percentage;
};
