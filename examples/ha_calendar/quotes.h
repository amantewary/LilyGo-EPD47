#pragma once

#include "epd_driver.h"
#include "types.h"
#include <Arduino.h>
#include <vector>

// Fetch quotes from Home Assistant and reset the current index to 0
void fetchQuotes(std::vector<QuoteData> &quotes, int &currentQuoteIndex);

// Draw the current quote within the given area (handles wrapping and truncation)
void drawQuote(const std::vector<QuoteData> &quotes, int currentQuoteIndex,
               const Rect_t &quoteArea);

// Advance to the next quote, power the display for drawing, and render it
void rotateQuote(std::vector<QuoteData> &quotes, int &currentQuoteIndex,
                 const Rect_t &quoteArea);
