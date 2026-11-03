// FLIGHT: one aircraft at a time. Page 0 = its route on a globe, page 1 = a few big numbers. Short = next aircraft, double = page.
#include "flight.h"
#include "display.h"
#include "settings.h"
#include "theme.h"
#include "adsb.h"
#include "auxdata.h"
#include "geo.h"
FlightMode flightMode;
static int sel = 0, page = 0; static float anim = 0; static char selIcao[7] = "";
void FlightMode::enter() { page = 0; }
void FlightMode::update(uint32_t, float dt) { anim += dt; }
bool FlightMode::onButton(Btn b) { if (b == Btn::Short) { sel++; selIcao[0] = 0; return true; } if (b == Btn::Double) { page ^= 1; return true; } return false; }

static Track* pick(Track* tr) {   // nearest-first list; keep following the same aircraft between frames
  int order[MAX_TRACKS], n = 0; for (int k = 0; k < MAX_TRACKS; k++) if (tr[k].valid && !tr[k].onGround) order[n++] = k;
  if (!n) return nullptr;
  for (int i = 1; i < n; i++) { int v = order[i], j = i - 1; while (j >= 0 && tr[order[j]].distKm > tr[v].distKm) { order[j + 1] = order[j]; j--; } order[j + 1] = v; }
  if (selIcao[0]) for (int i = 0; i < n; i++) if (!strcmp(tr[order[i]].icao, selIcao)) return &tr[order[i]];
  Track* t = &tr[order[sel % n]]; strlcpy(selIcao, t->icao, sizeof selIcao); return t;
}

static void drawGlobePage(Track& t, const RouteInfo* r, const Theme& T) {
  const int CX = 120, CY = 126; const float RV = 86;   // view radius
  bool hasRoute = r && r->from[0] && r->fromLat != 0;
  double la0 = t.dLat, lo0 = t.dLon; float R = RV;
  if (hasRoute) {
    la0 = (r->fromLat + r->toLat) / 2; lo0 = (r->fromLon + r->toLon) / 2; if (fabs(r->fromLon - r->toLon) > 180) lo0 += 180;
    // zoom so the route spans about 140 px: central angle between the endpoints
    double a1 = r->fromLat * DEG_TO_RAD, b1 = r->fromLon * DEG_TO_RAD, a2 = r->toLat * DEG_TO_RAD, b2 = r->toLon * DEG_TO_RAD;
    double d = acos(constrain(sin(a1) * sin(a2) + cos(a1) * cos(a2) * cos(b2 - b1), -1.0, 1.0));
    R = constrain((float)(70.0 / max(sin(d / 2), 0.02)), RV, 2500.0f);
  } else R = 600;  // no route: a regional view around the aircraft
  frame.drawCircle(CX, CY, RV, T.ring);
  // clip coastlines to the view circle by drawing into a temporary clip rect then masking the corners
  frame.setClipRect(CX - RV, CY - RV, 2 * RV + 1, 2 * RV + 1);
  geoDrawCoast(CX, CY, R, la0, lo0, T.ringDim);
  frame.clearClipRect();
  for (int yy = -RV; yy <= RV; yy++) { float w = sqrtf(RV * RV - yy * yy); frame.drawFastHLine(CX - RV, CY + yy, RV - w, T.bg); frame.drawFastHLine(CX + w + 1, CY + yy, RV - w, T.bg); }
  frame.drawCircle(CX, CY, RV, T.ring);
  auto inView = [&](float px, float py) { return (px - CX) * (px - CX) + (py - CY) * (py - CY) <= RV * RV; };
  float x, y;
  if (hasRoute) {
    static double arc[66]; geoGreatCircle(r->fromLat, r->fromLon, r->toLat, r->toLon, 32, arc);
    float px = 0, py = 0; bool pv = false;
    for (int i = 0; i <= 32; i++) { bool v = geoProject(arc[i * 2], arc[i * 2 + 1], la0, lo0, CX, CY, R, x, y) && inView(x, y); if (v && pv) frame.drawLine(px, py, x, y, T.vector); px = x; py = y; pv = v; }
    frame.setFont(&fonts::Font0); frame.setTextDatum(textdatum_t::middle_center);
    if (geoProject(r->fromLat, r->fromLon, la0, lo0, CX, CY, R, x, y) && inView(x, y)) { frame.fillCircle(x, y, 3, T.accent); frame.setTextColor(T.text); frame.drawString(r->from, x, y - 9); }
    if (geoProject(r->toLat, r->toLon, la0, lo0, CX, CY, R, x, y) && inView(x, y)) { frame.fillCircle(x, y, 3, T.accent); frame.setTextColor(T.text); frame.drawString(r->to, x, y - 9); }
  }
  if (geoProject(S.lat, S.lon, la0, lo0, CX, CY, R, x, y) && inView(x, y)) frame.drawCircle(x, y, 3, T.labelDim);   // you
  if (geoProject(t.dLat, t.dLon, la0, lo0, CX, CY, R, x, y) && inView(x, y)) {
    float pulse = 5 + 3 * fabsf(sinf(anim * 3)); frame.drawCircle(x, y, pulse, cLerp(T.bg, T.lock, 140));
    float a = t.headingDeg * DEG_TO_RAD; frame.fillTriangle(x + sinf(a) * 6, y - cosf(a) * 6, x + sinf(a + 2.5f) * 5, y - cosf(a + 2.5f) * 5, x + sinf(a - 2.5f) * 5, y - cosf(a - 2.5f) * 5, T.lock);
  }
  frame.setTextDatum(textdatum_t::top_center); frame.setFont(&fonts::Font2); frame.setTextColor(T.text); frame.drawString(t.callsign, CX, 6);
  frame.setFont(&fonts::Font0); frame.setTextColor(T.labelDim);
  frame.setTextDatum(textdatum_t::bottom_center);
  if (hasRoute) { String s = String(r->fromCity[0] ? r->fromCity : r->from) + " > " + (r->toCity[0] ? r->toCity : r->to); if (s.length() > 30) s = String(r->from) + " > " + r->to; frame.drawString(s, CX, 228); }
  else frame.drawString(r ? "route unknown" : "looking up route...", CX, 228);
}

static void drawDataPage(Track& t, const RouteInfo* r, const Theme& T) {
  const int CX = 120;
  frame.setTextDatum(textdatum_t::top_center);
  frame.setFont(&fonts::Orbitron_Light_24); frame.setTextColor(T.text); frame.drawString(t.callsign, CX, 22);
  frame.setFont(&fonts::Font2); frame.setTextColor(T.accent);
  if (r && r->from[0]) frame.drawString(String(r->from) + "  >  " + r->to, CX, 52); else frame.drawString(t.type[0] ? t.type : "----", CX, 52);
  frame.setFont(&fonts::Font0); frame.setTextColor(T.labelDim);
  String who = r && r->airline[0] ? String(r->airline) : String(t.desc[0] ? t.desc : t.type); if (who.length() > 26) who = who.substring(0, 26); frame.drawString(who, CX, 72);
  if (t.reg[0]) frame.drawString(t.reg, CX, 82);
  // three big figures: altitude, speed, distance
  char b[16];
  frame.setFont(&fonts::Font4); frame.setTextColor(T.text);
  snprintf(b, sizeof b, "%d", (int)t.altM); frame.setTextDatum(textdatum_t::top_right); frame.drawString(b, 112, 100);
  frame.setFont(&fonts::Font0); frame.setTextColor(T.labelDim); frame.setTextDatum(textdatum_t::top_left); frame.drawString(t.vrateMs > 1.5f ? "m ^" : (t.vrateMs < -1.5f ? "m v" : "m"), 116, 108);
  frame.setFont(&fonts::Font4); frame.setTextColor(T.text); frame.setTextDatum(textdatum_t::top_right); snprintf(b, sizeof b, "%d", (int)(t.speedMs * 3.6f)); frame.drawString(b, 112, 128);
  frame.setFont(&fonts::Font0); frame.setTextColor(T.labelDim); frame.setTextDatum(textdatum_t::top_left); frame.drawString("km/h", 116, 136);
  frame.setFont(&fonts::Font4); frame.setTextColor(T.text); frame.setTextDatum(textdatum_t::top_right); snprintf(b, sizeof b, "%.1f", t.distKm); frame.drawString(b, 112, 156);
  frame.setFont(&fonts::Font0); frame.setTextColor(T.labelDim); frame.setTextDatum(textdatum_t::top_left); snprintf(b, sizeof b, "km %s", t.bearingDeg < 22 || t.bearingDeg >= 338 ? "N" : t.bearingDeg < 67 ? "NE" : t.bearingDeg < 112 ? "E" : t.bearingDeg < 157 ? "SE" : t.bearingDeg < 202 ? "S" : t.bearingDeg < 247 ? "SW" : t.bearingDeg < 292 ? "W" : "NW"); frame.drawString(b, 116, 164);
  // closest approach line
  float tm, dm; frame.setTextDatum(textdatum_t::top_center); frame.setTextColor(T.lock);
  if (adsbClosestApproach(t, tm, dm)) { snprintf(b, sizeof b, "%.1f km in %d:%02d", dm, (int)tm / 60, (int)tm % 60); frame.drawString(String("closest ") + b, CX, 186); }
  else frame.drawString("moving away", CX, 186);
  // altitude trace (last ~50 s), tiny
  if (t.trCount > 2) {
    int lo = 32000, hi = 0; for (int i = 0; i < t.trCount; i++) { int idx = (t.trHead - t.trCount + i + TRAIL_LEN) % TRAIL_LEN; lo = min(lo, (int)t.trAlt[idx]); hi = max(hi, (int)t.trAlt[idx]); } if (hi - lo < 100) hi = lo + 100;
    float px = -1, py = -1; for (int i = 0; i < t.trCount; i++) { int idx = (t.trHead - t.trCount + i + TRAIL_LEN) % TRAIL_LEN; float x = 70 + 100.0f * i / (TRAIL_LEN - 1), y = 216 - 14.0f * (t.trAlt[idx] - lo) / (hi - lo); if (px >= 0) frame.drawLine(px, py, x, y, T.trail); px = x; py = y; }
  }
}

void FlightMode::draw() {
  const Theme& T = themeGet(S.theme); frame.fillSprite(T.bg);
  adsbLock(); Track* t = pick(adsbTracks());
  if (!t) { adsbUnlock(); frame.setTextDatum(textdatum_t::middle_center); frame.setFont(&fonts::Font2); frame.setTextColor(T.labelDim); frame.drawString("NO AIRCRAFT", 120, 120); return; }
  Track tc = *t; adsbUnlock();
  const RouteInfo* r = auxRoute(tc.callsign);
  if (page == 0) drawGlobePage(tc, r, T); else drawDataPage(tc, r, T);
  // page dots
  frame.fillCircle(114, 236, 2, page == 0 ? T.accent : T.ringDim); frame.fillCircle(126, 236, 2, page == 1 ? T.accent : T.ringDim);
}
