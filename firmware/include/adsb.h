#pragma once
#include <Arduino.h>
#define MAX_TRACKS 64
#define TRAIL_LEN 24

struct Track {
  char icao[7]; char callsign[9]; char type[5]; char reg[9]; char desc[24];
  double lat, lon;            // last API fix
  float altM, speedMs, headingDeg, vrateMs;
  bool onGround; bool valid;
  uint32_t tFix;              // millis of last API fix
  uint32_t firstSeen;
  // display state (smoothed / predicted)
  double dLat, dLon;
  // correction animation
  bool corrActive; uint32_t corrStart; double corrFromLat, corrFromLon;
  // trail of past display positions (lat/lon), ring buffer
  float trLat[TRAIL_LEN], trLon[TRAIL_LEN]; uint8_t trHead, trCount; uint32_t trLast;
  float distKm, bearingDeg;   // computed for current display position
};

enum class ApiStatus : uint8_t { NoData, Ok, NoPosition, Error, RateLimited, Paused, NoWifi };
struct AdsbStats {
  ApiStatus status = ApiStatus::NoData;
  int httpCode = 0; uint32_t lastOkMs = 0, lastReqMs = 0, durMs = 0, payload = 0;
  uint16_t raw = 0, valid = 0, active = 0, fGround = 0, fAlt = 0, fSpeed = 0, fRange = 0;
  uint32_t requestsToday = 0; uint32_t intervalMs = 10000;
  char provider[12] = "adsb.fi"; char err[48] = "";
};

void adsbBegin();                       // starts background task
void adsbLoop();                        // main-thread: prediction + housekeeping
void adsbRequestNow();                  // force an immediate fetch
Track* adsbTracks();                    // MAX_TRACKS array (valid flag)
const AdsbStats& adsbStats();
void adsbLock(); void adsbUnlock();     // guard track array when reading from another core
uint32_t adsbComputedInterval();
void adsbClearOpenSkyToken();
void adsbPause(bool on);                // e.g. during OTA upload
