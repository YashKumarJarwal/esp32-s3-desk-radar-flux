#include "clock.h"
#include "settings.h"
#include <time.h>
static bool started = false;
void clockBegin() { if (!started) { configTime(0, 0, "pool.ntp.org", "time.google.com"); started = true; Serial.println("[clock] NTP sync started"); } }
time_t clockNow() { return time(nullptr); }
bool clockSynced() { return time(nullptr) > 1700000000; }
void clockLocal(int& h, int& m, int& s) {
  time_t t = time(nullptr) + (time_t)S.tzMin * 60; struct tm tmv; gmtime_r(&t, &tmv); h = tmv.tm_hour; m = tmv.tm_min; s = tmv.tm_sec;
}
int clockLocalMinutes() { int h, m, s; clockLocal(h, m, s); return h * 60 + m; }
bool scheduleActive() {
  if (!S.scheduleEnabled) return true;
  if (!clockSynced()) return true;
  int now = clockLocalMinutes();
  if (S.startMin <= S.endMin) return now >= S.startMin && now < S.endMin;
  return now >= S.startMin || now < S.endMin;   // overnight window
}
String clockHHMM() { int h, m, s; clockLocal(h, m, s); char b[8]; snprintf(b, sizeof b, "%02d:%02d", h, m); return b; }
