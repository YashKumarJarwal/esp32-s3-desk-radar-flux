#pragma once
#include <Arduino.h>
// Sunrise/sunset in minutes UTC for a given date (NOAA simplified); returns false for polar day/night.
bool sunTimes(int year, int month, int day, double lat, double lon, float& riseMinUTC, float& setMinUTC);
float moonPhase(time_t t);   // 0..1 (0 = new, 0.5 = full)
