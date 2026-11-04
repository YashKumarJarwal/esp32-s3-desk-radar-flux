#include "globe.h"
#include "display.h"
#include "geo.h"
#include "geo_data.h"
#include <esp_heap_caps.h>
#include <time.h>
struct V3 { float x, y, z; };
static V3* coastV = nullptr; static V3* borderV = nullptr; static V3 sun = {1, 0, 0};
static inline V3 unitv(float latDeg, float lonDeg) { float la = latDeg * DEG_TO_RAD, lo = lonDeg * DEG_TO_RAD; float sla, cla, slo, clo; sincosf(la, &sla, &cla); sincosf(lo, &slo, &clo); return {cla * clo, cla * slo, sla}; }

void globeInit() {
  coastV = (V3*)heap_caps_malloc(GEO_COAST_POINTS * sizeof(V3), MALLOC_CAP_SPIRAM);
  borderV = (V3*)heap_caps_malloc(GEO_BORDER_POINTS * sizeof(V3), MALLOC_CAP_SPIRAM);
  for (int i = 0; i < GEO_COAST_POINTS; i++) coastV[i] = unitv((int16_t)pgm_read_word(&GEO_COAST[i][0]) / 100.0f, (int16_t)pgm_read_word(&GEO_COAST[i][1]) / 100.0f);
  for (int i = 0; i < GEO_BORDER_POINTS; i++) borderV[i] = unitv((int16_t)pgm_read_word(&GEO_BORDER[i][0]) / 100.0f, (int16_t)pgm_read_word(&GEO_BORDER[i][1]) / 100.0f);
}
void globeSetSun(time_t t) {
  struct tm u; gmtime_r(&t, &u); int doy = u.tm_yday; float hours = u.tm_hour + u.tm_min / 60.0f;
  float decl = 23.44f * sinf(TWO_PI * (doy - 81) / 365.0f); float sublon = 180.0f - 15.0f * hours;   // subsolar point (equation of time ignored: < 4 deg)
  sun = unitv(decl, sublon);
}
struct Basis { V3 c, e, n; };
static Basis basis(const GlobeCam& cam) {
  float la = cam.lat * DEG_TO_RAD, lo = cam.lon * DEG_TO_RAD; float sla, cla, slo, clo; sincosf(la, &sla, &cla); sincosf(lo, &slo, &clo);
  return {{cla * clo, cla * slo, sla}, {-slo, clo, 0}, {-sla * clo, -sla * slo, cla}};
}
static inline float dot(const V3& a, const V3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
bool globeProject(const GlobeCam& cam, double lat, double lon, int cx, int cy, float& x, float& y, float* depth) {
  Basis b = basis(cam); V3 p = unitv(lat, lon); float d = dot(p, b.c); x = cx + cam.R * dot(p, b.e); y = cy - cam.R * dot(p, b.n); if (depth) *depth = d; return d >= 0;
}
static void drawLines(const V3* pts, const uint16_t (*index)[2], int lines, const GlobeCam& cam, int cx, int cy, float Rv, uint16_t day, uint16_t night, bool closed) {
  Basis b = basis(cam); float R2 = Rv * Rv;
  for (int l = 0; l < lines; l++) {
    uint16_t start = pgm_read_word(&index[l][0]), n = pgm_read_word(&index[l][1]);
    float px = 0, py = 0; bool pv = false; int cnt = closed ? n + 1 : n;
    for (int i = 0; i < cnt; i++) {
      const V3& p = pts[start + (i % n)];
      float d = dot(p, b.c); bool v = d >= 0;
      float x = cx + cam.R * dot(p, b.e), y = cy - cam.R * dot(p, b.n);
      if (v && ((x - cx) * (x - cx) + (y - cy) * (y - cy)) > R2) v = false;
      if (v && pv) frame.drawLine(px, py, x, y, dot(p, sun) >= 0 ? day : night);
      px = x; py = y; pv = v;
    }
  }
}
void globeDrawCoast(const GlobeCam& c, int cx, int cy, float Rv, uint16_t day, uint16_t night) { if (coastV) drawLines(coastV, GEO_COAST_INDEX, GEO_COAST_RINGS, c, cx, cy, Rv, day, night, true); }
void globeDrawBorders(const GlobeCam& c, int cx, int cy, float Rv, uint16_t day, uint16_t night) { if (borderV) drawLines(borderV, GEO_BORDER_INDEX, GEO_BORDER_LINES, c, cx, cy, Rv, day, night, false); }

static bool inRing(int start, int n, float lat, float lon) {
  bool in = false;
  for (int i = 0, j = n - 1; i < n; j = i++) {
    float yi = (int16_t)pgm_read_word(&GEO_CPTS[start + i][0]) / 100.0f, xi = (int16_t)pgm_read_word(&GEO_CPTS[start + i][1]) / 100.0f;
    float yj = (int16_t)pgm_read_word(&GEO_CPTS[start + j][0]) / 100.0f, xj = (int16_t)pgm_read_word(&GEO_CPTS[start + j][1]) / 100.0f;
    if (((yi > lat) != (yj > lat)) && (lon < (xj - xi) * (lat - yi) / (yj - yi + 1e-9f) + xi)) in = !in;
  }
  return in;
}
const char* globeCountryAt(double lat, double lon) {
  static char name[16];
  for (int c = 0; c < GEO_COUNTRY_COUNT; c++) {
    GeoCountry gc; memcpy_P(&gc, &GEO_COUNTRIES[c], sizeof gc);
    for (int r = 0; r < gc.ringCount; r++) { uint16_t s = pgm_read_word(&GEO_CRINGS[gc.ringStart + r][0]), n = pgm_read_word(&GEO_CRINGS[gc.ringStart + r][1]); if (inRing(s, n, lat, lon)) { strlcpy(name, gc.name, sizeof name); return name; } }
  }
  return "";
}
int globeCitiesNear(double la1, double lo1, double la2, double lo2, int maxN, int* out) {
  static double arc[130]; geoGreatCircle(la1, lo1, la2, lo2, 64, arc); int n = 0;
  for (int i = 0; i < GEO_CITY_COUNT && n < maxN; i++) {   // cities are sorted by population, so the first hits are the biggest
    float la = (int16_t)pgm_read_word(&GEO_CITIES[i].lat) / 100.0f, lo = (int16_t)pgm_read_word(&GEO_CITIES[i].lon) / 100.0f; float best = 1e9;
    for (int k = 0; k <= 64; k++) { float dx = (lo - arc[k * 2 + 1]) * cosf(la * DEG_TO_RAD) * 111.32f, dy = (la - arc[k * 2]) * 110.57f; float d = dx * dx + dy * dy; if (d < best) best = d; }
    if (best < 200.0f * 200.0f) out[n++] = i;
  }
  return n;
}
void globeCity(int idx, double& lat, double& lon, char* name, size_t n) { GeoCity c; memcpy_P(&c, &GEO_CITIES[idx], sizeof c); lat = c.lat / 100.0; lon = c.lon / 100.0; strlcpy(name, c.name, n); }
