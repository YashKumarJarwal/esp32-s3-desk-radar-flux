#include "modes_extra.h"
#include "display.h"
#include "settings.h"
#include "theme.h"
#include "clock.h"
#include "astro.h"
#include "adsb.h"
#include "net.h"
#include <time.h>
ChronoMode chronoMode;
static float anim = 0;
void ChronoMode::update(uint32_t, float dt) { anim += dt; }

static void drawMoon(int x, int y, int r, float phase, uint16_t lit, uint16_t dark) {
  // phase 0 new, 0.5 full. Draw lit disc then overlay terminator with ellipse.
  frame.fillCircle(x, y, r, dark);
  float k = cosf(phase * TWO_PI); // 1 at new, -1 at full
  bool waxing = phase < 0.5f;
  // right half lit when waxing (northern hemisphere convention)
  for (int dy = -r; dy <= r; dy++) {
    float w = sqrtf((float)(r * r - dy * dy)); float e = w * k;   // terminator x
    int x0, x1;
    if (waxing) { x0 = (int)(x + e); x1 = (int)(x + w); } else { x0 = (int)(x - w); x1 = (int)(x - e); }
    if (x1 > x0) frame.drawFastHLine(x0, y + dy, x1 - x0, lit);
  }
}

void ChronoMode::draw() {
  const Theme& T = themeGet(S.theme);
  frame.fillSprite(T.bg);
  int h, m, s; clockLocal(h, m, s); time_t now = clockNow(); time_t lt = now + S.tzMin * 60; struct tm tmv; gmtime_r(&lt, &tmv);
  const int CX = 120, CY = 120;
  // day ring: 24h progress, with daylight arc highlighted
  float dayMin = h * 60 + m + s / 60.0f;
  float rise, set; bool hasSun = sunTimes(tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday, S.lat, S.lon, rise, set);
  frame.drawArc(CX, CY, 112, 116, 0, 360, T.ringDim);
  if (hasSun) {
    float r1 = fmodf(rise + S.tzMin + 1440, 1440), s1 = fmodf(set + S.tzMin + 1440, 1440);
    float a0 = r1 / 1440.0f * 360 - 90, a1 = s1 / 1440.0f * 360 - 90; if (a1 < a0) a1 += 360;
    frame.fillArc(CX, CY, 112, 116, a0, a1, cScale(T.accent, 45));
  }
  float aNow = dayMin / 1440.0f * 360 - 90;
  frame.fillArc(CX, CY, 110, 118, aNow - 2, aNow + 2, T.accent);
  // hour ticks
  for (int i = 0; i < 24; i++) { float a = (i / 24.0f * 360 - 90) * DEG_TO_RAD; int l = i % 6 == 0 ? 6 : 3; frame.drawLine(CX + cosf(a) * 104, CY + sinf(a) * 104, CX + cosf(a) * (104 - l), CY + sinf(a) * (104 - l), i % 6 == 0 ? T.ring : T.ringDim); }
  // seconds orbiter
  float as = (s / 60.0f * 360 - 90) * DEG_TO_RAD; frame.drawCircle(CX, CY, 96, T.ringDim); frame.fillCircle(CX + cosf(as) * 96, CY + sinf(as) * 96, 3, T.sweep);
  // big time
  frame.setTextDatum(textdatum_t::middle_center); frame.setFont(&fonts::Orbitron_Light_32); frame.setTextColor(T.text);
  char b[16]; snprintf(b, sizeof b, "%02d:%02d", h, m); frame.drawString(clockSynced() ? b : "--:--", CX, CY - 10);
  static const char* DOW[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"}; static const char* MON[] = {"JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"};
  frame.setFont(&fonts::Font2); frame.setTextColor(T.labelDim);
  snprintf(b, sizeof b, "%s %02d %s", DOW[tmv.tm_wday], tmv.tm_mday, MON[tmv.tm_mon]); frame.drawString(b, CX, CY + 18);
  // sunrise / sunset labels
  frame.setFont(&fonts::Font0); frame.setTextColor(T.labelDim);
  if (hasSun) { float r1 = fmodf(rise + S.tzMin + 1440, 1440), s1 = fmodf(set + S.tzMin + 1440, 1440); snprintf(b, sizeof b, "^%02d:%02d  v%02d:%02d", (int)r1 / 60, (int)r1 % 60, (int)s1 / 60, (int)s1 % 60); frame.drawString(b, CX, CY + 36); }
  // moon
  drawMoon(CX, CY + 66, 12, moonPhase(now), T.text, cScale(T.ring, 50));
  // aircraft count pill at the top
  const AdsbStats& st = adsbStats(); snprintf(b, sizeof b, "%u ACFT", st.active); frame.setTextColor(T.labelDim); frame.drawString(b, CX, CY - 52);
  frame.fillCircle(CX + 30, CY - 52, 2, netOnline() ? T.accent : T.warn);
}
