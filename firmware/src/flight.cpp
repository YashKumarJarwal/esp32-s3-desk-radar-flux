// FLIGHT: one aircraft at a time. Page 0 = animated globe with its route, page 1 = big figures, page 2 = route itinerary.
// Short = next aircraft, double = next page.
#include "flight.h"
#include "display.h"
#include "settings.h"
#include "theme.h"
#include "adsb.h"
#include "auxdata.h"
#include "geo.h"
#include "globe.h"
#include "clock.h"
FlightMode flightMode;
static int sel = 0, page = 0; static float anim = 0; static char selIcao[7] = "";
static uint32_t tSun = 0, tCountry = 0; static char overCountry[16] = "";
// per-route cache (recomputed when the followed flight's route appears)
static char cacheCs[9] = ""; static int cityIdx[6], cityN = 0; static char countries[8][16]; static int countryN = 0; static float routeAngle = 0;

void FlightMode::enter() { page = 0; globeSetSun(clockNow()); }
void FlightMode::update(uint32_t now, float dt) { anim += dt; if (now - tSun > 60000) { tSun = now; globeSetSun(clockNow()); } }
bool FlightMode::onButton(Btn b) { if (b == Btn::Short) { sel++; selIcao[0] = 0; return true; } if (b == Btn::Double) { page = (page + 1) % 3; return true; } return false; }

static Track* pick(Track* tr) {
  int order[MAX_TRACKS], n = 0; for (int k = 0; k < MAX_TRACKS; k++) if (tr[k].valid && !tr[k].onGround) order[n++] = k;
  if (!n) return nullptr;
  for (int i = 1; i < n; i++) { int v = order[i], j = i - 1; while (j >= 0 && tr[order[j]].distKm > tr[v].distKm) { order[j + 1] = order[j]; j--; } order[j + 1] = v; }
  if (selIcao[0]) for (int i = 0; i < n; i++) if (!strcmp(tr[order[i]].icao, selIcao)) return &tr[order[i]];
  Track* t = &tr[order[sel % n]]; strlcpy(selIcao, t->icao, sizeof selIcao); return t;
}
static void slerp(double la1, double lo1, double la2, double lo2, float f, double& la, double& lo) {
  static double pts[4]; double a1 = la1 * DEG_TO_RAD, b1 = lo1 * DEG_TO_RAD, a2 = la2 * DEG_TO_RAD, b2 = lo2 * DEG_TO_RAD;
  double x1 = cos(a1) * cos(b1), y1 = cos(a1) * sin(b1), z1 = sin(a1), x2 = cos(a2) * cos(b2), y2 = cos(a2) * sin(b2), z2 = sin(a2);
  double d = acos(constrain(x1 * x2 + y1 * y2 + z1 * z2, -1.0, 1.0)); if (d < 1e-6) { la = la1; lo = lo1; return; }
  double A = sin((1 - f) * d) / sin(d), B = sin(f * d) / sin(d); double x = A * x1 + B * x2, y = A * y1 + B * y2, z = A * z1 + B * z2;
  la = atan2(z, sqrt(x * x + y * y)) * RAD_TO_DEG; lo = atan2(y, x) * RAD_TO_DEG;
}
static float angleBetween(double la1, double lo1, double la2, double lo2) {
  double a1 = la1 * DEG_TO_RAD, b1 = lo1 * DEG_TO_RAD, a2 = la2 * DEG_TO_RAD, b2 = lo2 * DEG_TO_RAD;
  return acos(constrain(sin(a1) * sin(a2) + cos(a1) * cos(a2) * cos(b2 - b1), -1.0, 1.0));
}
static void buildRouteCache(const RouteInfo* r) {
  if (!r || !r->from[0] || r->fromLat == 0) { cacheCs[0] = 0; return; }
  if (!strcmp(cacheCs, r->callsign)) return;
  strlcpy(cacheCs, r->callsign, sizeof cacheCs);
  cityN = globeCitiesNear(r->fromLat, r->fromLon, r->toLat, r->toLon, 6, cityIdx);
  routeAngle = angleBetween(r->fromLat, r->fromLon, r->toLat, r->toLon);
  countryN = 0; static double arc[98]; geoGreatCircle(r->fromLat, r->fromLon, r->toLat, r->toLon, 48, arc);
  for (int i = 0; i <= 48 && countryN < 8; i++) { const char* c = globeCountryAt(arc[i * 2], arc[i * 2 + 1]); if (!c[0]) continue; if (countryN && !strcmp(countries[countryN - 1], c)) continue; bool dup = false; for (int k = 0; k < countryN; k++) if (!strcmp(countries[k], c)) dup = true; if (!dup) strlcpy(countries[countryN++], c, 16); }
}

static void drawGlobePage(Track& t, const RouteInfo* r, const Theme& T) {
  const int CX = 120, CY = 122; const float RV = 100;   // view radius: the globe nearly fills the screen
  bool hasRoute = r && r->from[0] && r->fromLat != 0;
  buildRouteCache(hasRoute ? r : nullptr);
  GlobeCam cam;
  if (hasRoute) {
    float u = 0.5f - 0.5f * cosf(anim * TWO_PI / 28.0f);              // camera glides departure -> arrival -> departure, 28 s cycle
    slerp(r->fromLat, r->fromLon, r->toLat, r->toLon, u, cam.lat, cam.lon);
    cam.R = constrain(80.0f / max(sinf(routeAngle / 2), 0.02f), 96.0f, 2500.0f);   // full globe for long-haul, zoomed-in for short hops
  } else { cam.lat = t.dLat; cam.lon = t.dLon + 25.0f * sinf(anim / 6.0f); cam.R = 96; }
  // atmosphere + night-side disc
  for (int i = 3; i >= 1; i--) frame.drawCircle(CX, CY, RV + i * 2, cLerp(T.bg, T.ring, 40 * i));
  frame.fillCircle(CX, CY, RV, cLerp(T.bg, T.ringDim, 25));
  uint16_t day = cLerp(T.bg, T.text, 170), night = cLerp(T.bg, T.ring, 150);
  globeDrawCoast(cam, CX, CY, RV, day, night);
  globeDrawBorders(cam, CX, CY, RV, cLerp(T.bg, T.ringDim, 200), cLerp(T.bg, T.ringDim, 110));
  frame.drawCircle(CX, CY, RV, T.ring);
  float x, y, dpt;
  frame.setFont(&fonts::Font0); frame.setTextDatum(textdatum_t::middle_center);
  if (hasRoute) {
    // route: flown part bright, remaining part dim
    static double arc[66]; geoGreatCircle(r->fromLat, r->fromLon, r->toLat, r->toLon, 32, arc);
    float flown = angleBetween(r->fromLat, r->fromLon, t.dLat, t.dLon) / max(routeAngle, 1e-4f); if (flown > 1) flown = 1;
    float px = 0, py = 0; bool pv = false;
    for (int i = 0; i <= 32; i++) { bool v = globeProject(cam, arc[i * 2], arc[i * 2 + 1], CX, CY, x, y) && (x - CX) * (x - CX) + (y - CY) * (y - CY) <= RV * RV; if (v && pv) frame.drawLine(px, py, x, y, i / 32.0f <= flown ? T.lock : T.vector); px = x; py = y; pv = v; }
    // intermediate cities: dots always, names only near the view centre
    float fx = -99, fy = -99, tx = -99, ty = -99; globeProject(cam, r->fromLat, r->fromLon, CX, CY, fx, fy); globeProject(cam, r->toLat, r->toLon, CX, CY, tx, ty);
    for (int i = 0; i < cityN; i++) {
      double la, lo; char nm[14]; globeCity(cityIdx[i], la, lo, nm, sizeof nm);
      if (!strncasecmp(nm, r->fromCity, 6) || !strncasecmp(nm, r->toCity, 6)) continue;          // endpoints already labelled
      if (!globeProject(cam, la, lo, CX, CY, x, y, &dpt) || (x - CX) * (x - CX) + (y - CY) * (y - CY) > RV * RV) continue;
      if (fabsf(x - fx) < 30 && fabsf(y - fy) < 14) continue; if (fabsf(x - tx) < 30 && fabsf(y - ty) < 14) continue;
      frame.fillCircle(x, y, 1, T.labelDim);
      if (dpt > 0.80f) { frame.setTextColor(cLerp(T.bg, T.labelDim, 255 * (dpt - 0.8f) / 0.2f)); frame.drawString(nm, x, y + 8); }
    }
    auto inV = [&](float px, float py) { return (px - CX) * (px - CX) + (py - CY) * (py - CY) <= RV * RV; };
    if (globeProject(cam, r->fromLat, r->fromLon, CX, CY, x, y) && inV(x, y)) { frame.fillCircle(x, y, 3, T.accent); frame.setTextColor(T.text); frame.drawString(r->from, x, y - 9); }
    if (globeProject(cam, r->toLat, r->toLon, CX, CY, x, y) && inV(x, y)) { frame.fillCircle(x, y, 3, T.accent); frame.setTextColor(T.text); frame.drawString(r->to, x, y + 10); }
  }
  auto inView = [&](float px, float py) { return (px - CX) * (px - CX) + (py - CY) * (py - CY) <= RV * RV; };
  if (globeProject(cam, S.lat, S.lon, CX, CY, x, y) && inView(x, y)) frame.drawCircle(x, y, 2, T.labelDim);   // you
  if (globeProject(cam, t.dLat, t.dLon, CX, CY, x, y) && inView(x, y)) {
    float pulse = 5 + 3 * fabsf(sinf(anim * 3)); frame.drawCircle(x, y, pulse, cLerp(T.bg, T.lock, 160));
    float a = t.headingDeg * DEG_TO_RAD; frame.fillTriangle(x + sinf(a) * 6, y - cosf(a) * 6, x + sinf(a + 2.5f) * 5, y - cosf(a + 2.5f) * 5, x + sinf(a - 2.5f) * 5, y - cosf(a - 2.5f) * 5, T.lock);
  }
  // HUD: callsign on top, the country under the camera at the bottom
  frame.fillRoundRect(80, 4, 80, 16, 4, T.bg); frame.setTextDatum(textdatum_t::top_center); frame.setFont(&fonts::Font2); frame.setTextColor(T.text); frame.drawString(t.callsign, CX, 4);
  if (millis() - tCountry > 700) { tCountry = millis(); strlcpy(overCountry, globeCountryAt(cam.lat, cam.lon), sizeof overCountry); }
  frame.fillRoundRect(40, 221, 160, 11, 4, T.bg); frame.setTextDatum(textdatum_t::bottom_center); frame.setFont(&fonts::Font0); frame.setTextColor(T.labelDim);
  if (hasRoute) frame.drawString(overCountry[0] ? overCountry : (String(r->from) + " > " + r->to), CX, 231); else frame.drawString(r ? "route unknown" : "looking up route...", CX, 231);
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
  char b[16];
  auto big = [&](int y, const char* val, const char* unit) { frame.setFont(&fonts::Font4); frame.setTextColor(T.text); frame.setTextDatum(textdatum_t::top_right); frame.drawString(val, 112, y); frame.setFont(&fonts::Font0); frame.setTextColor(T.labelDim); frame.setTextDatum(textdatum_t::top_left); frame.drawString(unit, 116, y + 8); };
  snprintf(b, sizeof b, "%d", (int)t.altM); big(100, b, t.vrateMs > 1.5f ? "m ^" : (t.vrateMs < -1.5f ? "m v" : "m"));
  snprintf(b, sizeof b, "%d", (int)(t.speedMs * 3.6f)); big(128, b, "km/h");
  snprintf(b, sizeof b, "%.1f", t.distKm); const char* dir = t.bearingDeg < 22 || t.bearingDeg >= 338 ? "N" : t.bearingDeg < 67 ? "NE" : t.bearingDeg < 112 ? "E" : t.bearingDeg < 157 ? "SE" : t.bearingDeg < 202 ? "S" : t.bearingDeg < 247 ? "SW" : t.bearingDeg < 292 ? "W" : "NW"; char u[8]; snprintf(u, sizeof u, "km %s", dir); big(156, b, u);
  float tm, dm; frame.setTextDatum(textdatum_t::top_center); frame.setTextColor(T.lock); frame.setFont(&fonts::Font0);
  if (adsbClosestApproach(t, tm, dm)) { snprintf(b, sizeof b, "%.1f km in %d:%02d", dm, (int)tm / 60, (int)tm % 60); frame.drawString(String("closest ") + b, CX, 186); } else frame.drawString("moving away", CX, 186);
  if (t.trCount > 2) {
    int lo = 32000, hi = 0; for (int i = 0; i < t.trCount; i++) { int idx = (t.trHead - t.trCount + i + TRAIL_LEN) % TRAIL_LEN; lo = min(lo, (int)t.trAlt[idx]); hi = max(hi, (int)t.trAlt[idx]); } if (hi - lo < 100) hi = lo + 100;
    float px = -1, py = -1; for (int i = 0; i < t.trCount; i++) { int idx = (t.trHead - t.trCount + i + TRAIL_LEN) % TRAIL_LEN; float x = 70 + 100.0f * i / (TRAIL_LEN - 1), y = 216 - 14.0f * (t.trAlt[idx] - lo) / (hi - lo); if (px >= 0) frame.drawLine(px, py, x, y, T.trail); px = x; py = y; }
  }
}

static void drawRoutePage(Track& t, const RouteInfo* r, const Theme& T) {
  const int CX = 120; frame.setTextDatum(textdatum_t::top_center);
  frame.setFont(&fonts::Font2); frame.setTextColor(T.text); frame.drawString(t.callsign, CX, 14);
  if (!(r && r->from[0] && r->fromLat != 0)) { frame.setFont(&fonts::Font0); frame.setTextColor(T.labelDim); frame.drawString("route unknown", CX, 110); return; }
  buildRouteCache(r);
  float flown = angleBetween(r->fromLat, r->fromLon, t.dLat, t.dLon) / max(routeAngle, 1e-4f); if (flown > 1) flown = 1;
  int y0 = 46, y1 = 196; frame.drawFastVLine(40, y0, y1 - y0, T.ringDim); frame.drawFastVLine(40, y0, (y1 - y0) * flown, T.lock);
  frame.fillCircle(40, y0, 4, T.accent); frame.fillCircle(40, y1, 4, T.accent); frame.fillCircle(40, y0 + (y1 - y0) * flown, 3, T.lock);
  frame.setTextDatum(textdatum_t::middle_left);
  frame.setFont(&fonts::Font2); frame.setTextColor(T.text); frame.drawString(r->from, 52, y0); frame.drawString(r->to, 52, y1);
  frame.setFont(&fonts::Font0); frame.setTextColor(T.labelDim); frame.drawString(r->fromCity, 90, y0); frame.drawString(r->toCity, 90, y1);
  // countries crossed, evenly spaced along the line
  for (int i = 0; i < countryN; i++) { int y = y0 + (y1 - y0) * (i + 1) / (countryN + 1); frame.fillCircle(40, y, 2, T.ringDim); frame.setTextColor(T.labelDim); frame.drawString(countries[i], 52, y); }
  frame.setTextDatum(textdatum_t::top_center); frame.setTextColor(T.labelDim);
  char b[32]; snprintf(b, sizeof b, "%d%% flown   %d km total", (int)(flown * 100), (int)(routeAngle * 6371)); frame.drawString(b, CX, 212);
}

void FlightMode::draw() {
  const Theme& T = themeGet(S.theme); frame.fillSprite(T.bg);
  adsbLock(); Track* t = pick(adsbTracks());
  if (!t) { adsbUnlock(); frame.setTextDatum(textdatum_t::middle_center); frame.setFont(&fonts::Font2); frame.setTextColor(T.labelDim); frame.drawString("NO AIRCRAFT", 120, 120); return; }
  Track tc = *t; adsbUnlock();
  const RouteInfo* r = auxRoute(tc.callsign);
  if (page == 0) drawGlobePage(tc, r, T); else if (page == 1) drawDataPage(tc, r, T); else drawRoutePage(tc, r, T);
  for (int i = 0; i < 3; i++) frame.fillCircle(108 + i * 12, 236, 2, page == i ? T.accent : T.ringDim);
}
