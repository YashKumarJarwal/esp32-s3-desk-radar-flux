#include "geo.h"
#include "settings.h"
#include "display.h"
#include "geo_data.h"
#define NEAR_MAX 24
static NearAirport near_[NEAR_MAX]; static int nearN = 0;
static double cLat = 999, cLon = 999; static float cRange = 0; static uint32_t tRefresh = 0;

static float kmBetween(double la1, double lo1, double la2, double lo2) {
  float dx = (lo2 - lo1) * cosf(la1 * DEG_TO_RAD) * 111.32f, dy = (la2 - la1) * 110.57f; return sqrtf(dx * dx + dy * dy);
}
void geoLoop() {
  uint32_t now = millis();
  if (fabs(cLat - S.lat) < 1e-6 && fabs(cLon - S.lon) < 1e-6 && cRange == S.rangeKm && now - tRefresh < 60000) return;
  cLat = S.lat; cLon = S.lon; cRange = S.rangeKm; tRefresh = now; nearN = 0;
  float lim = max(S.rangeKm * 1.5f, 40.0f); float dlat = lim / 110.57f;
  for (int i = 0; i < GEO_AIRPORT_COUNT; i++) {
    GeoAirport a; memcpy_P(&a, &GEO_AIRPORTS[i], sizeof a);
    double la = a.lat / 1e5, lo = a.lon / 1e5; if (fabs(la - S.lat) > dlat) continue;
    float d = kmBetween(S.lat, S.lon, la, lo); if (d > lim) continue;
    int at = nearN < NEAR_MAX ? nearN++ : -1;
    if (at < 0) { float worst = 0; for (int k = 0; k < NEAR_MAX; k++) if (near_[k].distKm > worst) { worst = near_[k].distKm; at = k; } if (d >= worst) continue; }
    NearAirport& n = near_[at]; n.idx = i; n.distKm = d; n.lat = la; n.lon = lo; strlcpy(n.icao, a.icao, 5); strlcpy(n.iata, a.iata, 4);
  }
}
const NearAirport* geoNearby(int& count) { count = nearN; return near_; }
const NearAirport* geoNearestTo(double lat, double lon, float maxKm) {
  const NearAirport* best = nullptr; float bd = maxKm;
  for (int i = 0; i < nearN; i++) { float d = kmBetween(lat, lon, near_[i].lat, near_[i].lon); if (d < bd) { bd = d; best = &near_[i]; } }
  return best;
}
void geoDrawRunways(void (*toXY)(double, double, float&, float&), uint16_t col, uint16_t labelCol, bool labels) {
  for (int i = 0; i < nearN; i++) {
    GeoAirport a; memcpy_P(&a, &GEO_AIRPORTS[near_[i].idx], sizeof a);
    float ax, ay; toXY(near_[i].lat, near_[i].lon, ax, ay);
    for (int r = 0; r < a.rwCount; r++) {
      GeoRunway rw; memcpy_P(&rw, &GEO_RUNWAYS[a.rwStart + r], sizeof rw);
      float x0, y0, x1, y1; toXY(rw.lat0 / 1e5, rw.lon0 / 1e5, x0, y0); toXY(rw.lat1 / 1e5, rw.lon1 / 1e5, x1, y1);
      if (fabsf(x1 - x0) + fabsf(y1 - y0) < 3) { frame.drawPixel(x0, y0, col); continue; }
      frame.drawWideLine(x0, y0, x1, y1, 1.2f, col);
    }
    if (labels && ax > 12 && ax < 228 && ay > 12 && ay < 228) { frame.setFont(&fonts::Font0); frame.setTextDatum(textdatum_t::top_center); frame.setTextColor(labelCol); frame.drawString(a.iata[0] ? a.iata : a.icao, ax, ay + 5); }
  }
}
bool geoProject(double lat, double lon, double lat0, double lon0, int cx, int cy, float R, float& x, float& y) {
  double la = lat * DEG_TO_RAD, lo = lon * DEG_TO_RAD, la0 = lat0 * DEG_TO_RAD, lo0 = lon0 * DEG_TO_RAD;
  double cosc = sin(la0) * sin(la) + cos(la0) * cos(la) * cos(lo - lo0);
  x = cx + R * cos(la) * sin(lo - lo0); y = cy - R * (cos(la0) * sin(la) - sin(la0) * cos(la) * cos(lo - lo0));
  return cosc >= 0;
}
void geoDrawCoast(int cx, int cy, float R, double lat0, double lon0, uint16_t col) {
  for (int r = 0; r < GEO_COAST_RINGS; r++) {
    uint16_t start = pgm_read_word(&GEO_COAST_INDEX[r][0]), n = pgm_read_word(&GEO_COAST_INDEX[r][1]);
    float px = 0, py = 0; bool pv = false;
    for (int i = 0; i <= n; i++) {
      int k = start + (i % n); double la = (int16_t)pgm_read_word(&GEO_COAST[k][0]) / 100.0, lo = (int16_t)pgm_read_word(&GEO_COAST[k][1]) / 100.0;
      float x, y; bool v = geoProject(la, lo, lat0, lon0, cx, cy, R, x, y);
      if (v && pv) frame.drawLine(px, py, x, y, col);
      px = x; py = y; pv = v;
    }
  }
}
void geoGreatCircle(double la1, double lo1, double la2, double lo2, int n, double* out) {
  double a1 = la1 * DEG_TO_RAD, b1 = lo1 * DEG_TO_RAD, a2 = la2 * DEG_TO_RAD, b2 = lo2 * DEG_TO_RAD;
  double x1 = cos(a1) * cos(b1), y1 = cos(a1) * sin(b1), z1 = sin(a1), x2 = cos(a2) * cos(b2), y2 = cos(a2) * sin(b2), z2 = sin(a2);
  double d = acos(constrain(x1 * x2 + y1 * y2 + z1 * z2, -1.0, 1.0)); if (d < 1e-6) d = 1e-6;
  for (int i = 0; i <= n; i++) {
    double f = (double)i / n, A = sin((1 - f) * d) / sin(d), B = sin(f * d) / sin(d);
    double x = A * x1 + B * x2, y = A * y1 + B * y2, z = A * z1 + B * z2;
    out[i * 2] = atan2(z, sqrt(x * x + y * y)) * RAD_TO_DEG; out[i * 2 + 1] = atan2(y, x) * RAD_TO_DEG;
  }
}
