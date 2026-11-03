#include "sky.h"
#include "display.h"
#include "settings.h"
#include "theme.h"
#include "adsb.h"
#include "clock.h"
#include "net.h"
#include "auxdata.h"

SkyMode skyMode;
static float sweep = 0;           // degrees
static const int CX = 120, CY = 120, R = 104;
static int lockIdx = -1; static uint32_t lockSince = 0; static float lockAnim = 0;

static void toXY(double lat, double lon, float& x, float& y) {
  float dx = (lon - S.lon) * cosf(S.lat * DEG_TO_RAD) * 111.32f, dy = (lat - S.lat) * 110.57f;
  x = CX + dx / S.rangeKm * R; y = CY - dy / S.rangeKm * R;
}
void SkyMode::enter() { sweep = 0; }
void SkyMode::update(uint32_t now, float dt) { sweep += dt * 90.0f; if (sweep >= 360) sweep -= 360; lockAnim += dt; }

void SkyMode::nextRange() {
  int idx = 0; float best = 1e9;
  for (int i = 0; i < 3; i++) { float d = fabsf(S.preset[i] - S.rangeKm); if (d < best) { best = d; idx = i; } }
  idx = (idx + 1) % 3; S.rangeKm = S.preset[idx];
  S.fetchKm = max(S.fetchKm, S.rangeKm); S.fetchKm = min(S.fetchKm, min(S.rangeKm * 1.5f, 300.0f));
  settingsSave(); adsbRequestNow();
  overlayToast("RANGE " + String((int)S.rangeKm) + " KM");
}
void SkyMode::nextTheme() { S.theme = (S.theme + 1) % THEME_COUNT; settingsSave(); overlayToast(themeGet(S.theme).name); }

bool SkyMode::onButton(Btn b) {
  if (b == Btn::Short) { nextTheme(); return true; }
  if (b == Btn::Double) { nextRange(); return true; }
  return false;
}

static void drawPlane(float x, float y, float hdg, uint16_t col, float sz) {
  float a = hdg * DEG_TO_RAD; float ca = cosf(a), sa = sinf(a);
  auto rot = [&](float px, float py, float& ox, float& oy) { ox = x + px * ca - py * sa; oy = y + px * sa + py * ca; };
  // arrow pointing "up" (north) before rotation: tip (0,-sz), tail (-0.7sz, +0.8sz), (0.7sz,+0.8sz), notch (0,+0.4sz)
  float x0, y0, x1, y1, x2, y2, x3, y3;
  rot(0, -sz, x0, y0); rot(-0.75f * sz, 0.85f * sz, x1, y1); rot(0, 0.4f * sz, x3, y3); rot(0.75f * sz, 0.85f * sz, x2, y2);
  frame.fillTriangle(x0, y0, x1, y1, x3, y3, col); frame.fillTriangle(x0, y0, x3, y3, x2, y2, col);
}

static void drawRadarBase(const Theme& T) {
  frame.fillSprite(T.bg);
  if (T.scanlines) for (int y = 0; y < 240; y += 3) frame.drawFastHLine(0, y, 240, cScale(T.bg, 60));
  // sweep wedge (trailing gradient)
  for (int i = 8; i >= 1; i--) {
    uint16_t c = cLerp(T.bg, T.sweepDim, 255 * (9 - i) / 9);
    frame.fillArc(CX, CY, 0, R, sweep - i * 5, sweep - (i - 1) * 5, c);
  }
  frame.drawLine(CX, CY, CX + cosf(sweep * DEG_TO_RAD) * R, CY + sinf(sweep * DEG_TO_RAD) * R, T.sweep);
  // rings + crosshair
  for (int i = 1; i <= 3; i++) frame.drawCircle(CX, CY, R * i / 3, i == 3 ? T.ring : T.ringDim);
  frame.drawFastHLine(CX - R, CY, 2 * R, T.ringDim); frame.drawFastVLine(CX, CY - R, 2 * R, T.ringDim);
  // compass ticks
  for (int a = 0; a < 360; a += 30) {
    float r = a % 90 == 0 ? 8 : 4; float ca = cosf((a - 90) * DEG_TO_RAD), sa = sinf((a - 90) * DEG_TO_RAD);
    frame.drawLine(CX + ca * (R + 2), CY + sa * (R + 2), CX + ca * (R + 2 + r), CY + sa * (R + 2 + r), a % 90 == 0 ? T.ring : T.ringDim);
  }
  frame.setFont(&fonts::Font0); frame.setTextDatum(textdatum_t::middle_center); frame.setTextColor(T.labelDim);
  frame.drawString("N", CX, CY - R - 10); frame.drawString("S", CX, CY + R + 10); frame.drawString("E", CX + R + 10, CY); frame.drawString("W", CX - R - 10, CY);
  frame.fillCircle(CX, CY, 2, T.accent);
}

static void drawHud(const Theme& T, int shown, int total) {
  const AdsbStats& st = adsbStats();
  frame.setFont(&fonts::Font2); frame.setTextDatum(textdatum_t::top_center); frame.setTextColor(T.text);
  frame.drawString(String((int)S.rangeKm) + " KM", CX, 24);
  frame.setTextDatum(textdatum_t::bottom_center);
  frame.drawString(clockSynced() ? clockHHMM() : "--:--", CX, 222);
  frame.fillRect(80, 204, 80, 20, T.bg);
  frame.setFont(&fonts::Font0); frame.setTextColor(T.labelDim);
  frame.drawString(String(shown) + "/" + String(total) + " ACFT", CX, 208);
  // status
  const char* msg = nullptr; uint16_t col = T.warn;
  switch (st.status) {
    case ApiStatus::NoData: msg = "NO API DATA"; break;
    case ApiStatus::Error: msg = "API ERROR"; break;
    case ApiStatus::RateLimited: msg = "RATE LIMITED"; break;
    case ApiStatus::NoWifi: msg = "NO WIFI"; break;
    case ApiStatus::Paused: msg = "PAUSED"; break;
    default: break;
  }
  if (msg) { frame.setTextDatum(textdatum_t::top_center); frame.setTextColor(col); frame.drawString(msg, CX, 42); }
  // data-source + link pips
  uint16_t pip = st.status == ApiStatus::Ok && millis() - st.lastOkMs < st.intervalMs * 3 ? T.accent : T.warn;
  frame.fillCircle(CX + 40, 31, 2, pip); frame.fillCircle(CX - 40, 31, 2, netOnline() ? T.accent : T.warn);
}

void SkyMode::draw() {
  const Theme& T = themeGet(S.theme);
  drawRadarBase(T);
  adsbLock();
  Track* tr = adsbTracks();
  int order[MAX_TRACKS]; int n = 0;
  for (int k = 0; k < MAX_TRACKS; k++) if (tr[k].valid) order[n++] = k;
  for (int i = 1; i < n; i++) { int v = order[i], j = i - 1; while (j >= 0 && tr[order[j]].distKm > tr[v].distKm) { order[j + 1] = order[j]; j--; } order[j + 1] = v; }
  float ext = min(S.edgeExtKm, min(S.rangeKm * 0.5f, 300 - S.rangeKm));
  int shown = 0, edge = 0; int nearest = -1;
  struct Box { int x0, y0, x1, y1; }; Box boxes[MAX_TRACKS]; int nb = 0;
  auto free = [&](int x0, int y0, int x1, int y1) { for (int q = 0; q < nb; q++) if (x0 < boxes[q].x1 && x1 > boxes[q].x0 && y0 < boxes[q].y1 && y1 > boxes[q].y0) return false; return true; };
  // trails first (under everything)
  if (S.trails) for (int i = 0; i < n && shown < S.maxAircraft; i++) {
    Track& t = tr[order[i]]; if (t.distKm > S.rangeKm) continue;
    float px = -1, py = -1;
    for (int j = 0; j < t.trCount; j++) {
      int idx = (t.trHead - t.trCount + j + TRAIL_LEN) % TRAIL_LEN; float x, y; toXY(t.trLat[idx], t.trLon[idx], x, y);
      if (px >= 0) frame.drawLine(px, py, x, y, cLerp(T.bg, T.trail, 60 + 195 * j / TRAIL_LEN));
      px = x; py = y;
    }
    shown++;
  }
  shown = 0;
  for (int i = 0; i < n; i++) {
    Track& t = tr[order[i]];
    if (t.distKm <= S.rangeKm) {
      if (shown >= S.maxAircraft) continue;
      float x, y; toXY(t.dLat, t.dLon, x, y);
      uint16_t col = t.onGround ? T.labelDim : (t.altM > 9000 ? T.aircraftAlt : T.aircraft);
      // speed vector = 90 s of travel
      float vKm = t.speedMs * 90 / 1000.0f * R / S.rangeKm; if (vKm > 2 && !t.onGround)
        frame.drawLine(x, y, x + sinf(t.headingDeg * DEG_TO_RAD) * vKm, y - cosf(t.headingDeg * DEG_TO_RAD) * vKm, T.vector);
      drawPlane(x, y, t.headingDeg, col, 5.5f);
      if (S.labels && free(x - 24, y - 16, x + 24, y + 26)) {
        boxes[nb++] = {(int)x - 24, (int)y - 16, (int)x + 24, (int)y + 26};
        frame.setFont(&fonts::Font0); frame.setTextDatum(textdatum_t::bottom_center); frame.setTextColor(T.label);
        frame.drawString(t.callsign, x, y - 8);
        frame.setTextDatum(textdatum_t::top_center); frame.setTextColor(T.labelDim);
        char b[20]; snprintf(b, sizeof b, "%s %d", t.type[0] ? t.type : "---", (int)(t.speedMs * 3.6f)); frame.drawString(b, x, y + 8);
        snprintf(b, sizeof b, "%dm%s", (int)t.altM, t.vrateMs > 1.5f ? "^" : (t.vrateMs < -1.5f ? "v" : "")); frame.drawString(b, x, y + 17);
      }
      if (nearest < 0 && !t.onGround) nearest = order[i];
      shown++;
    } else if (S.edgeDots && t.distKm <= S.rangeKm + ext) {
      float a = (t.bearingDeg - 90) * DEG_TO_RAD; frame.fillCircle(CX + cosf(a) * (R + 5), CY + sinf(a) * (R + 5), 2, T.edge); edge++;
    }
  }
  // lock-on reticle + card for nearest airborne aircraft
  if (S.lockOn && nearest >= 0) {
    Track& t = tr[nearest]; float x, y; toXY(t.dLat, t.dLon, x, y);
    if (nearest != lockIdx) { lockIdx = nearest; lockSince = millis(); }
    float grow = min(1.0f, (millis() - lockSince) / 600.0f); float rr = 14 + (1 - grow) * 20; float rot = lockAnim * 1.2f;
    for (int q = 0; q < 4; q++) {
      float a0 = rot + q * HALF_PI; float a1 = a0 + 0.45f;
      frame.drawLine(x + cosf(a0) * rr, y + sinf(a0) * rr, x + cosf(a1) * rr, y + sinf(a1) * rr, T.lock);
    }
    frame.fillRoundRect(28, 183, 184, 20, 5, T.bg); frame.drawRoundRect(28, 183, 184, 20, 5, cScale(T.lock, 35));
    frame.setFont(&fonts::Font0); frame.setTextDatum(textdatum_t::top_center); frame.setTextColor(T.lock);
    char b[40]; snprintf(b, sizeof b, "%s  %.1f KM  %d KM/H", t.callsign, t.distKm, (int)(t.speedMs * 3.6f)); frame.drawString(b, CX, 186);
    const RouteInfo* ri = auxRoute(t.callsign);
    frame.setTextColor(T.labelDim);
    if (ri && ri->from[0]) snprintf(b, sizeof b, "%s %s>%s", ri->airline[0] ? ri->airline : t.type, ri->from, ri->to);
    else snprintf(b, sizeof b, "%s %s", t.type, t.desc[0] ? t.desc : t.reg);
    frame.drawString(b, CX, 195);
    // overhead alert
    if (t.distKm < S.overheadAlertKm && !t.onGround) {
      uint8_t ph = (uint8_t)(128 + 127 * sinf(lockAnim * 6)); frame.drawCircle(CX, CY, R + 10, cLerp(T.bg, T.warn, ph)); frame.drawCircle(CX, CY, R + 11, cLerp(T.bg, T.warn, ph));
      frame.setFont(&fonts::Font2); frame.setTextColor(T.warn); frame.drawString("OVERHEAD", CX, 44);
    }
  } else lockIdx = -1;
  adsbUnlock();
  drawHud(T, shown, n);
}

void drawIdleClock() {
  const Theme& T = themeGet(S.theme);
  frame.fillSprite(T.bg);
  int h, m, s; clockLocal(h, m, s);
  frame.drawArc(CX, CY, R + 6, R + 8, -90, -90 + 360.0f * (h % 12 * 60 + m) / 720.0f, T.accent);
  frame.drawArc(CX, CY, R - 2, R, -90, -90 + 6.0f * s, T.ringDim);
  frame.setFont(&fonts::Orbitron_Light_32); frame.setTextDatum(textdatum_t::middle_center); frame.setTextColor(T.text);
  char b[8]; snprintf(b, sizeof b, "%02d:%02d", h, m); frame.drawString(b, CX, CY - 8);
  frame.setFont(&fonts::Font2); frame.setTextColor(T.labelDim); frame.drawString("SCHEDULE OFF", CX, CY + 28);
}
