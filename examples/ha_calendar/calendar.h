#pragma once

#include "types.h"
#include <vector>

// Fetch upcoming calendar events from Home Assistant
void fetchCalendar(std::vector<CalendarEvent> &calendarEvents);
