#include "modes_extra.h"
#include "display.h"
#include "settings.h"
#include "theme.h"
#include "auxdata.h"
#include "clock.h"
OrbitMode orbitMode;
static float anim = 0;
void OrbitMode::update(uint32_t, float dt) { anim += dt; }

// orthographic projection of lat/lon onto a globe centred on the observer
static bool proj(double lat, double lon, double lat0, double lon0, float R, float& x, float& y) {
  double la = lat * DEG_TO_RAD, lo = lon * DEG_TO_RAD, la0 = lat0 * DEG_TO_RAD, lo0 = lon0 * DEG_TO_RAD;
  double cosc = sin(la0) * sin(la) + cos(la0) * cos(la) * cos(lo - lo0);
  x = 120 + R * cos(la) * sin(lo - lo0); y = 120 - R * (cos(la0) * sin(la) - sin(la0) * cos(la) * cos(lo - lo0));
  return cosc >= 0;
}
void OrbitMode::draw() {
  const Theme& T = themeGet(S.theme); const IssFix& I = auxIss();
  frame.fillSprite(T.bg); const int CX = 120, CY = 120; const float R = 92;
  // globe wireframe
  frame.drawCircle(CX, CY, R, T.ring);
  for (int lat = -60; lat <= 60; lat += 30) { float px = -1, py = -1; for (int lon = -180; lon <= 180; lon += 6) { float x, y; bool v = proj(lat, lon, S.lat, S.lon, R, x, y); if (v && px >= 0) frame.drawLine(px, py, x, y, T.ringDim); px = v ? x : -1; py = y; } }
  for (int lon = -180; lon < 180; lon += 30) { float px = -1, py = -1; for (int lat = -90; lat <= 90; lat += 6) { float x, y; bool v = proj(lat, lon, S.lat, S.lon, R, x, y); if (v && px >= 0) frame.drawLine(px, py, x, y, T.ringDim); px = v ? x : -1; py = y; } }
  frame.fillCircle(CX, CY, 3, T.accent); // observer at centre
  frame.setTextDatum(textdatum_t::middle_center); frame.setFont(&fonts::Font0);
  if (!I.valid) { frame.setTextColor(T.labelDim); frame.drawString("locating ISS...", CX, CY + 110); return; }
  // ground track trail
  int c, h; const double* tr = auxIssTrail(c, h); float px = -1, py = -1;
  for (int i = 0; i < c; i++) { int idx = (h - c + i + ISS_TRAIL) % ISS_TRAIL; float x, y; bool v = proj(tr[idx * 2], tr[idx * 2 + 1], S.lat, S.lon, R, x, y); if (v && px >= 0) frame.drawLine(px, py, x, y, cLerp(T.bg, T.trail, 80 + 175 * i / max(1, c))); px = v ? x : -1; py = y; }
  float x, y; bool vis = proj(I.lat, I.lon, S.lat, S.lon, R, x, y);
  if (vis) {
    float pulse = 4 + 3 * fabsf(sinf(anim * 3));
    frame.drawCircle(x, y, pulse + 4, cLerp(T.bg, T.lock, 120)); frame.fillCircle(x, y, 3, T.lock);
    frame.drawLine(x - 9, y, x - 4, y, T.lock); frame.drawLine(x + 4, y, x + 9, y, T.lock); // solar panels
    frame.setTextColor(T.lock); frame.drawString("ISS", x, y - 10);
  } else { // below horizon: show bearing on rim
    float a = (I.bearing - 90) * DEG_TO_RAD; frame.fillCircle(CX + cosf(a) * (R + 8), CY + sinf(a) * (R + 8), 3, T.lock);
  }
  // HUD
  frame.setFont(&fonts::Font2); frame.setTextColor(T.text); frame.drawString("ISS", CX, 14);
  frame.setFont(&fonts::Font0); frame.setTextColor(T.labelDim);
  char b[48]; snprintf(b, sizeof b, "%.0f KM AWAY  ALT %.0f KM", I.distKm, I.altKm); frame.drawString(b, CX, 226);
  snprintf(b, sizeof b, "%.0f KM/H  %s", I.velKmh, I.visibility); frame.drawString(b, CX, 214);
  if (I.elevDeg > 0) { frame.setTextColor(T.warn); snprintf(b, sizeof b, "ABOVE HORIZON  %.0f deg  %s", I.elevDeg, !strcmp(I.visibility, "visible") ? "LOOK UP!" : ""); frame.drawString(b, CX, 28); }
}
