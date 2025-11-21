#pragma once

// Home Assistant Entity Configuration
// Copy this file to config.h and update with your own entity IDs
// config.h is NOT tracked by git

// Weather entity
#define ENTITY_WEATHER "weather.your_weather_entity"

// Quote sensor (optional)
#define ENTITY_QUOTE "sensor.quote_of_the_day"

// Todo entities - add your todo entity IDs here
#define ENTITY_TODOS_COUNT 2
#define ENTITY_TODO_1 "todo.your_todo_1"
#define ENTITY_TODO_2 "todo.your_todo_2"

// Calendar entities - add your calendar entity IDs here
#define ENTITY_CALENDARS_COUNT 2
#define ENTITY_CALENDAR_1 "calendar.your_calendar_1"
#define ENTITY_CALENDAR_2 "calendar.your_calendar_2"

// NTP Timezone Configuration
#define NTP_SERVER "pool.ntp.org"
#define GMT_OFFSET_SEC -18000   // UTC-5 (EST). Adjust for your timezone (UTC offset in seconds)
#define DAYLIGHT_OFFSET_SEC 3600 // 1 hour for DST

