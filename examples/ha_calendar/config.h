#pragma once

// Home Assistant Entity Configuration
// This file is NOT tracked by git - create your own config.h from config.example.h

// Weather entity
#define ENTITY_WEATHER "weather.toronto_forecast"

// Quote sensor (optional)
#define ENTITY_QUOTE "sensor.quote_of_the_day"

// Todo entities - add your todo entity IDs here
#define ENTITY_TODOS_COUNT 3
#define ENTITY_TODO_1 "todo.errands"
#define ENTITY_TODO_2 "todo.work"
#define ENTITY_TODO_3 "todo.personal"

// Calendar entities - add your calendar entity IDs here
#define ENTITY_CALENDARS_COUNT 2
#define ENTITY_CALENDAR_1 "calendar.aman_outlook_calendar"
#define ENTITY_CALENDAR_2 "calendar.home_2"

// NTP Timezone Configuration
#define NTP_SERVER "pool.ntp.org"
#define GMT_OFFSET_SEC -18000   // UTC-5 (EST). Adjust for your timezone (UTC offset in seconds)
#define DAYLIGHT_OFFSET_SEC 3600 // 1 hour for DST

