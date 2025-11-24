#pragma once

// Home Assistant Entity Configuration
// This file is NOT tracked by git - create your own config.h from config.example.h

// Weather entity
#define ENTITY_WEATHER "weather.toronto_forecast"

// Quote sensor (optional)
#define ENTITY_QUOTE "sensor.quote_of_the_day"

// Todo entities - add your todo entity IDs here
#define ENTITY_TODOS_COUNT 4
#define ENTITY_TODO_1 "todo.errands"
#define ENTITY_TODO_2 "todo.growth"
#define ENTITY_TODO_3 "todo.personal"
#define ENTITY_TODO_4 "todo.work"


// Calendar entities - add your calendar entity IDs here
#define ENTITY_CALENDARS_COUNT 6
#define ENTITY_CALENDAR_1 "calendar.family_calendar"
#define ENTITY_CALENDAR_2 "calendar.home_2"
#define ENTITY_CALENDAR_3 "calendar.home_assistant_ms365_calendar"
#define ENTITY_CALENDAR_4 "calendar.home_assistant_ms365_family"
#define ENTITY_CALENDAR_5 "calendar.home_assistant_ms365_home"
#define ENTITY_CALENDAR_6 "calendar.home_assistant_ms365_work"

// NTP Timezone Configuration
#define NTP_SERVER "pool.ntp.org"
#define GMT_OFFSET_SEC -18000   // UTC-5 (EST). Adjust for your timezone (UTC offset in seconds)
#define DAYLIGHT_OFFSET_SEC 3600 // 1 hour for DST

// Home Assistant connection
// Set to 1 to use HTTPS (recommended if your HA is served over SSL)
#define HA_USE_HTTPS 0

// Display preferences
#define USE_FAHRENHEIT 0 // Set to 1 to convert weather temperature to °F
#define USE_24H_TIME 0   // Set to 1 for 24-hour clock

// Deep sleep interval (minutes) when running in battery-optimized mode
#define SLEEP_INTERVAL_MINUTES 60
