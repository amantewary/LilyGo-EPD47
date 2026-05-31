#pragma once

#include "epd_driver.h"
#include <Arduino.h>
#include <vector>

// Word wrap helper used by both list rendering and quotes
std::vector<String> wrapText(const String &text, int maxChars);

// Draw bitmap icons using pre-defined data
void drawBitmapIcon(int32_t x, int32_t y, const uint8_t *icon_data,
                    uint32_t width, uint32_t height);

// Draw a dashed divider line
void drawTextDivider(int32_t x, int32_t y, int32_t width);

// Generic list renderer (todos/events) with optional icons and wrapping
void drawList(const Rect_t &area, const std::vector<String> &lines,
              String header, bool drawIcons = false, int lineSpacing = 35,
              const uint8_t *headerIconData = NULL,
              uint32_t headerIconWidth = 0, uint32_t headerIconHeight = 0);
