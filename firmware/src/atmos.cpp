#include "modes_extra.h"
#include "display.h"
#include "settings.h"
#include "theme.h"
#include "clock.h"
#include "auxdata.h"
AtmosMode atmosMode;
static float anim = 0; static int page = 0;
void AtmosMode::update(uint32_t, float dt) { anim += dt; }
bool AtmosMode::onButton(Btn b) { if (b == Btn::Short) { page ^= 1; return true; } return false; }

static const char* wmoText(int c) {
  if (c == 0) return "CLEAR"; if (c <= 2) return "PARTLY CLOUDY"; if (c == 3) return "OVERCAST"; if (c <= 48) return "FOG"; if (c <= 57) return "DRIZZLE";
  if (c <= 67) return "RAIN"; if (c <= 77) return "SNOW"; if (c <= 82) return "SHOWERS"; if (c <= 86) return "SNOW SHOWERS"; return "THUNDERSTORM";
}
static uint16_t aqiColor(int a) { if (a <= 50) return c565(0, 228, 0); if (a <= 100) return c565(255, 255, 0); if (a <= 150) return c565(255, 126, 0); if (a <= 200) return c565(255, 0, 0); if (a <= 300) return c565(143, 63, 151); return c565(126, 0, 35); }
static const char* aqiText(int a) { if (a <= 50) return "GOOD"; if (a <= 100) return "MODERATE"; if (a <= 150) return "UNHEALTHY*"; if (a <= 200) return "UNHEALTHY"; if (a <= 300) return "VERY UNHEALTHY"; return "HAZARDOUS"; }

static void glyph(int code, int isDay, int x, int y, uint16_t col, uint16_t dim, float t) {
  if (code == 0 || code == 1) { // sun / moon
    if (isDay) { frame.fillCircle(x, y, 9, col); for (int i = 0; i < 8; i++) { float a = i * PI / 4 + t * 0.3f; frame.drawLine(x + cosf(a) * 13, y + sinf(a) * 13, x + cosf(a) * 17, y + sinf(a) * 17, col); } }
    else { frame.fillCircle(x, y, 9, col); frame.fillCircle(x + 5, y - 3, 8, themeGet(S.theme).bg); }
  }
  if (code >= 1 && code <= 48) { // cloud
    frame.fillCircle(x - 6, y + 4, 7, dim); frame.fillCircle(x + 2, y, 9, dim); frame.fillCircle(x + 9, y + 5, 6, dim); frame.fillRect(x - 6, y + 4, 15, 7, dim);
  }
  if (code >= 51 && code <= 82) for (int i = 0; i < 3; i++) { float ph = fmodf(t * 2 + i * 0.33f, 1.0f); frame.drawLine(x - 6 + i * 6, y + 12 + ph * 10, x - 7 + i * 6, y + 16 + ph * 10, col); }
  if (code >= 95) frame.drawLine(x - 2, y + 8, x + 2, y + 14, col), frame.drawLine(x + 2, y + 14, x - 1, y + 20, col);
}

void AtmosMode::draw() {
  const Theme& T = themeGet(S.theme); const Weather& W = auxWeather();
  frame.fillSprite(T.bg); const int CX = 120, CY = 120;
  frame.setTextDatum(textdatum_t::middle_center);
  if (!W.valid) { frame.setFont(&fonts::Font2); frame.setTextColor(T.labelDim); frame.drawString("ATMOS", CX, CY - 10); frame.setFont(&fonts::Font0); frame.drawString("fetching open-meteo...", CX, CY + 12); return; }
  // AQI gauge around the rim: 270 degrees from 135 to 405
  uint16_t ac = aqiColor(W.aqi);
  frame.drawArc(CX, CY, 110, 118, 135, 405, T.ringDim);
  float frac = constrain(W.aqi / 300.0f, 0.0f, 1.0f); frame.fillArc(CX, CY, 110, 118, 135, 135 + 270 * frac, ac);
  frame.setFont(&fonts::Font0); frame.setTextColor(T.labelDim); frame.drawString("AQI", CX, 226);
  frame.setTextColor(ac); frame.setFont(&fonts::Font2); frame.drawString(String(W.aqi) + "  " + aqiText(W.aqi), CX, 212);
  if (page == 0) {
    glyph(W.code, W.isDay, CX, 48, T.accent, cScale(T.text, 55), anim);
    frame.setFont(&fonts::Orbitron_Light_32); frame.setTextColor(T.text); { String ts = String((int)roundf(W.tempC)); frame.drawString(ts, CX - 6, 100); int tw = frame.textWidth(ts); frame.drawCircle(CX - 6 + tw / 2 + 8, 88, 3, T.text); frame.drawCircle(CX - 6 + tw / 2 + 8, 88, 2, T.text); }
    frame.setFont(&fonts::Font2); frame.setTextColor(T.labelDim); frame.drawString(wmoText(W.code), CX, 128);
    frame.setFont(&fonts::Font0);
    char b[48]; snprintf(b, sizeof b, "FEELS %dC   HUM %d%%   WIND %d", (int)roundf(W.feelsC), (int)W.humidity, (int)W.windKmh); frame.drawString(b, CX, 148);
    snprintf(b, sizeof b, "PM2.5 %.0f   PM10 %.0f", W.pm25, W.pm10); frame.drawString(b, CX, 162);
    // hourly temperature sparkline + rain probability bars
    if (W.hours) {
      float mn = 1e9, mx = -1e9; for (int i = 0; i < W.hours; i++) { mn = min(mn, W.hourT[i]); mx = max(mx, W.hourT[i]); } if (mx - mn < 2) mx = mn + 2;
      int x0 = 50, w = 140, yb = 196, hgt = 18; float px = -1, py = -1;
      for (int i = 0; i < W.hours; i++) {
        float x = x0 + w * i / (W.hours - 1.0f), y = yb - (W.hourT[i] - mn) / (mx - mn) * hgt;
        if (W.hourP[i] > 0) frame.drawFastVLine(x, yb - W.hourP[i] * hgt / 100, W.hourP[i] * hgt / 100, cLerp(T.bg, T.vector, 110));
        if (px >= 0) frame.drawLine(px, py, x, y, T.text); px = x; py = y;
      }
      int hh = clockLocalMinutes() / 60; float xn = x0 + w * hh / 23.0f; frame.drawFastVLine(xn, yb - hgt - 2, hgt + 4, T.warn);
    }
  } else {
    frame.setFont(&fonts::Font2); frame.setTextColor(T.text); frame.drawString("NEXT 24H", CX, 50);
    frame.setFont(&fonts::Font0);
    int hh = clockLocalMinutes() / 60;
    for (int k = 0; k < 8; k++) { int i = (hh + k * 3) % 24; char b[32]; snprintf(b, sizeof b, "%02d:00   %3.0fC   %3d%%", i, W.hourT[i], W.hourP[i]); frame.setTextColor(k == 0 ? T.accent : T.labelDim); frame.drawString(b, CX, 70 + k * 15); }
  }
}
