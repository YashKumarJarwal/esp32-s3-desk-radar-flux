#pragma once
#include <Arduino.h>
struct Theme {
  const char* name;
  uint16_t bg, ring, ringDim, sweep, sweepDim, aircraft, aircraftAlt, vector, label, labelDim, text, accent, warn, edge, trail, lock;
  bool scanlines;
};
#define THEME_COUNT 4
const Theme& themeGet(uint8_t idx);
uint16_t c565(uint8_t r, uint8_t g, uint8_t b);
uint16_t cScale(uint16_t c, uint8_t pct);           // dim a colour to pct%
uint16_t cLerp(uint16_t a, uint16_t b, uint8_t t);  // t 0..255
