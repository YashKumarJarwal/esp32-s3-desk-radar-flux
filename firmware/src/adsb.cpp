#include "adsb.h"
#include "settings.h"
#include "net.h"
#include "clock.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <esp_heap_caps.h>
#include <base64.h>
#include "auxdata.h"

static Track* tracks;        // in PSRAM
static AdsbStats stats;
static SemaphoreHandle_t mtx;
static volatile bool forceNow = false; static volatile bool paused = false;
void adsbPause(bool on) { paused = on; }
static String osToken; static uint32_t osTokenExp = 0;
static int lastDay = -1;

struct PsramAllocator : ArduinoJson::Allocator {
  void* allocate(size_t n) override { return heap_caps_malloc(n, MALLOC_CAP_SPIRAM); }
  void deallocate(void* p) override { heap_caps_free(p); }
  void* reallocate(void* p, size_t n) override { return heap_caps_realloc(p, n, MALLOC_CAP_SPIRAM); }
};
static PsramAllocator psAlloc;

void adsbLock() { xSemaphoreTake(mtx, portMAX_DELAY); }
void adsbUnlock() { xSemaphoreGive(mtx); }
Track* adsbTracks() { return tracks; }
const AdsbStats& adsbStats() { return stats; }
void adsbRequestNow() { forceNow = true; }
void adsbClearOpenSkyToken() { osToken = ""; osTokenExp = 0; }

static float kmBetween(double la1, double lo1, double la2, double lo2) {
  float dx = (lo2 - lo1) * cosf(la1 * DEG_TO_RAD) * 111.32f, dy = (la2 - la1) * 110.57f;
  return sqrtf(dx * dx + dy * dy);
}
static float bearingTo(double la1, double lo1, double la2, double lo2) {
  float dx = (lo2 - lo1) * cosf(la1 * DEG_TO_RAD), dy = (la2 - la1);
  float b = atan2f(dx, dy) * RAD_TO_DEG; return b < 0 ? b + 360 : b;
}
static float effectiveFetchKm() { float f = max(S.fetchKm, S.rangeKm); return min(f, min(S.rangeKm * 1.5f, 300.0f)); }

uint32_t adsbComputedInterval() {
  if (S.refreshPolicy == 1) return max(S.manualIntervalMs, S.minIntervalMs);
  // AutoByDailyBudget: spread (budget*reserve)/cost requests over the active window
  float activeMin = S.scheduleEnabled ? ((S.endMin - S.startMin + 1440) % 1440) : 1440;
  if (activeMin < 1) activeMin = 1440;
  float reqs = max(1.0f, S.dailyBudget * S.reserve / max(S.cost, 0.01f));
  uint32_t iv = (uint32_t)(activeMin * 60000.0f / reqs);
  return max(iv, S.minIntervalMs);
}

struct Fix { char icao[7]; char cs[9]; char type[5]; char reg[9]; char desc[24]; double lat, lon; float altM, spd, hdg, vr; bool ground; };

static void applyFixes(Fix* fx, int n, uint32_t now) {
  adsbLock();
  uint16_t matched = 0, fresh = 0, corr = 0, jumps = 0;
  stats.fGround = stats.fAlt = stats.fSpeed = stats.fRange = 0;
  float fetch = effectiveFetchKm();
  for (int i = 0; i < n; i++) {
    Fix& f = fx[i];
    if (f.ground && !S.showGround) { stats.fGround++; continue; }
    if (!f.ground && f.altM < S.minAltM) { stats.fAlt++; continue; }
    if (!f.ground && f.spd < S.minSpeedMs) { stats.fSpeed++; continue; }
    if (kmBetween(S.lat, S.lon, f.lat, f.lon) > fetch) { stats.fRange++; continue; }
    Track* t = nullptr; Track* slot = nullptr;
    for (int k = 0; k < MAX_TRACKS; k++) { if (tracks[k].valid && !strcmp(tracks[k].icao, f.icao)) { t = &tracks[k]; break; } if (!tracks[k].valid && !slot) slot = &tracks[k]; }
    if (!t) {
      if (!slot) continue;
      t = slot; memset(t, 0, sizeof(Track)); t->valid = true; t->firstSeen = now; strcpy(t->icao, f.icao);
      t->dLat = f.lat; t->dLon = f.lon; fresh++;
    } else {
      matched++;
      float err = kmBetween(t->dLat, t->dLon, f.lat, f.lon);
      if (err > S.jumpKm) { t->dLat = f.lat; t->dLon = f.lon; t->corrActive = false; jumps++; t->trCount = 0; }
      else if (S.correction && err > S.corrStartKm && now - t->tFix >= S.corrMinApiMs) {
        t->corrActive = true; t->corrStart = now; t->corrFromLat = t->dLat; t->corrFromLon = t->dLon; corr++;
      } else if (!S.prediction) { t->dLat = f.lat; t->dLon = f.lon; }
      else { t->dLat += (f.lat - t->dLat) * S.alpha; t->dLon += (f.lon - t->dLon) * S.alpha; }
    }
    strncpy(t->callsign, f.cs, 8); strncpy(t->type, f.type, 4); strncpy(t->reg, f.reg, 8); strncpy(t->desc, f.desc, 23);
    t->lat = f.lat; t->lon = f.lon; t->altM = f.altM; t->speedMs = f.spd; t->headingDeg = f.hdg; t->vrateMs = f.vr; t->onGround = f.ground; t->tFix = now;
  }
  uint16_t active = 0; for (int k = 0; k < MAX_TRACKS; k++) if (tracks[k].valid) active++;
  stats.active = active;
  adsbUnlock();
  if (S.serialDebug) Serial.printf("[adsb] matched=%u new=%u corrections=%u jumps=%u active=%u filtered g/a/s/r=%u/%u/%u/%u\n", matched, fresh, corr, jumps, active, stats.fGround, stats.fAlt, stats.fSpeed, stats.fRange);
}

static bool fetchAdsbFi(uint32_t now) {
  float fetch = effectiveFetchKm(); int nm = (int)roundf(fetch / 1.852f);
  String url = "https://opendata.adsb.fi/api/v3/lat/" + String(S.lat, 5) + "/lon/" + String(S.lon, 5) + "/dist/" + String(nm);
  WiFiClientSecure c; c.setInsecure(); c.setTimeout(8000);
  HTTPClient http; http.setUserAgent("FLUX/" FLUX_VERSION); http.setTimeout(9000); http.setReuse(false); http.useHTTP10(true);
  if (!http.begin(c, url)) { strcpy(stats.err, "begin failed"); return false; }
  http.addHeader("Connection", "close");
  uint32_t t0 = millis(); int code = http.GET(); stats.httpCode = code; stats.durMs = millis() - t0;
  strlcpy(stats.provider, "adsb.fi", sizeof stats.provider);
  if (code == 429) { stats.status = ApiStatus::RateLimited; strcpy(stats.err, "rate limited"); http.end(); return false; }
  if (code != 200) { stats.status = ApiStatus::Error; snprintf(stats.err, sizeof stats.err, "HTTP %d", code); http.end(); return false; }
  JsonDocument filter; filter["ac"][0]["hex"] = true; filter["ac"][0]["flight"] = true; filter["ac"][0]["t"] = true;
  filter["ac"][0]["lat"] = true; filter["ac"][0]["lon"] = true; filter["ac"][0]["alt_baro"] = true;
  filter["ac"][0]["gs"] = true; filter["ac"][0]["track"] = true; filter["ac"][0]["baro_rate"] = true; filter["ac"][0]["r"] = true; filter["ac"][0]["desc"] = true;
  JsonDocument doc(&psAlloc);
  String body = http.getString(); http.end();
  stats.payload = body.length();
  auto err = deserializeJson(doc, body, DeserializationOption::Filter(filter));
  body = String();
  if (err) { stats.status = ApiStatus::Error; snprintf(stats.err, sizeof stats.err, "json %s", err.c_str()); return false; }
  JsonArray arr = doc["ac"].as<JsonArray>();
  static Fix fx[160]; int n = 0; stats.raw = arr.size(); stats.valid = 0;
  for (JsonObject a : arr) {
    if (n >= 160) break;
    if (!a["lat"].is<double>() || !a["lon"].is<double>()) continue;
    Fix& f = fx[n]; memset(&f, 0, sizeof f);
    strlcpy(f.icao, a["hex"] | "", 7); strlcpy(f.cs, a["flight"] | "", 9); strlcpy(f.type, a["t"] | "", 5); strlcpy(f.reg, a["r"] | "", 9); strlcpy(f.desc, a["desc"] | "", 24);
    for (int i = 7; i >= 0 && f.cs[i] == ' '; i--) f.cs[i] = 0; // trim
    if (!f.cs[0]) strlcpy(f.cs, f.icao, 9);
    f.lat = a["lat"]; f.lon = a["lon"];
    if (a["alt_baro"].is<const char*>()) { f.ground = true; f.altM = 0; } else f.altM = (float)(a["alt_baro"] | 0.0) * 0.3048f;
    f.spd = (float)(a["gs"] | 0.0) * 0.514444f; f.hdg = a["track"] | 0.0f; f.vr = (float)(a["baro_rate"] | 0.0) * 0.00508f;
    n++; stats.valid++;
  }
  applyFixes(fx, n, now);
  stats.status = ApiStatus::Ok; stats.lastOkMs = now; stats.err[0] = 0;
  return true;
}

static bool openSkyAuth(String& hdr) {
  if (S.account == 0) return true;                       // anonymous
  if (S.osClientId.length() && S.osClientSecret.length()) {
    if (osToken.length() && millis() < osTokenExp) { hdr = "Bearer " + osToken; return true; }
    WiFiClientSecure c; c.setInsecure(); HTTPClient http;
    http.begin(c, "https://auth.opensky-network.org/auth/realms/opensky-network/protocol/openid-connect/token");
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    int code = http.POST("grant_type=client_credentials&client_id=" + S.osClientId + "&client_secret=" + S.osClientSecret);
    if (code != 200) { snprintf(stats.err, sizeof stats.err, "token HTTP %d", code); http.end(); return false; }
    JsonDocument d; deserializeJson(d, http.getStream()); http.end();
    osToken = d["access_token"].as<String>(); osTokenExp = millis() + ((d["expires_in"] | 1800) - 60) * 1000UL;
    hdr = "Bearer " + osToken; return osToken.length() > 0;
  }
  if (S.osUser.length()) { hdr = "Basic " + base64::encode(S.osUser + ":" + S.osPass); return true; }
  return true;
}

static bool fetchOpenSky(uint32_t now) {
  float fetch = effectiveFetchKm(); double dlat = fetch / 110.57, dlon = fetch / (111.32 * cos(S.lat * DEG_TO_RAD));
  String url = "https://opensky-network.org/api/states/all?lamin=" + String(S.lat - dlat, 4) + "&lomin=" + String(S.lon - dlon, 4) + "&lamax=" + String(S.lat + dlat, 4) + "&lomax=" + String(S.lon + dlon, 4);
  String auth; if (!openSkyAuth(auth)) { stats.status = ApiStatus::Error; return false; }
  WiFiClientSecure c; c.setInsecure(); c.setTimeout(10000); HTTPClient http; http.setTimeout(12000); http.setReuse(false); http.useHTTP10(true);
  if (!http.begin(c, url)) return false;
  http.addHeader("Connection", "close");
  if (auth.length()) http.addHeader("Authorization", auth);
  uint32_t t0 = millis(); int code = http.GET(); stats.httpCode = code; stats.durMs = millis() - t0;
  strlcpy(stats.provider, "OpenSky", sizeof stats.provider);
  if (code == 429) { stats.status = ApiStatus::RateLimited; http.end(); return false; }
  if (code != 200) { stats.status = ApiStatus::Error; snprintf(stats.err, sizeof stats.err, "HTTP %d", code); http.end(); return false; }
  JsonDocument doc(&psAlloc); String body = http.getString(); http.end(); stats.payload = body.length();
  auto err = deserializeJson(doc, body); body = String();
  if (err) { stats.status = ApiStatus::Error; return false; }
  JsonArray st = doc["states"].as<JsonArray>(); static Fix fx[160]; int n = 0; stats.raw = st.size(); stats.valid = 0;
  for (JsonArray s : st) {
    if (n >= 160) break; if (s[6].isNull() || s[5].isNull()) continue;
    Fix& f = fx[n]; memset(&f, 0, sizeof f);
    strlcpy(f.icao, s[0] | "", 7); strlcpy(f.cs, s[1] | "", 9); for (int i = 7; i >= 0 && f.cs[i] == ' '; i--) f.cs[i] = 0; if (!f.cs[0]) strlcpy(f.cs, f.icao, 9);
    f.lon = s[5]; f.lat = s[6]; f.altM = s[7] | 0.0f; f.ground = s[8] | false; f.spd = s[9] | 0.0f; f.hdg = s[10] | 0.0f; f.vr = s[11] | 0.0f;
    n++; stats.valid++;
  }
  applyFixes(fx, n, now);
  stats.status = ApiStatus::Ok; stats.lastOkMs = now; stats.err[0] = 0; return true;
}

static void task(void*) {
  unsigned long backoff = 0;
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(200));
    if (paused) continue;
    if (!netOnline()) { stats.status = ApiStatus::NoWifi; continue; }
    if (!scheduleActive()) { stats.status = ApiStatus::Paused; continue; }
    uint32_t now = millis(); stats.intervalMs = adsbComputedInterval();
    bool due = forceNow || (now - stats.lastReqMs >= stats.intervalMs + backoff);
    if (!due) continue;
    forceNow = false; stats.lastReqMs = now;
    int h, m, s; clockLocal(h, m, s); time_t tnow = clockNow(); int day = (int)((tnow + S.tzMin * 60) / 86400);
    if (day != lastDay) { lastDay = day; stats.requestsToday = 0; }
    stats.requestsToday++;
    httpsLock(); bool ok = S.provider == Provider::AdsbFi ? fetchAdsbFi(now) : fetchOpenSky(now); httpsUnlock();
    if (!ok && S.provider == Provider::AdsbFi && stats.status != ApiStatus::RateLimited) { Serial.printf("[adsb] adsb.fi failed (%s), trying OpenSky fallback\n", stats.err); httpsLock(); ok = fetchOpenSky(now); httpsUnlock(); }
    if (S.serialDebug || !ok) Serial.printf("[adsb] %s HTTP=%d dur=%ums payload=%u raw=%u valid=%u active=%u status=%d %s\n", stats.provider, stats.httpCode, stats.durMs, stats.payload, stats.raw, stats.valid, stats.active, (int)stats.status, stats.err);
    backoff = ok ? 0 : (stats.status == ApiStatus::RateLimited ? 30000 : min(backoff + 5000UL, 60000UL));
  }
}

void adsbBegin() {
  tracks = (Track*)heap_caps_calloc(MAX_TRACKS, sizeof(Track), MALLOC_CAP_SPIRAM);
  mtx = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(task, "adsb", 12288, nullptr, 0, nullptr, 0);  // prio 0: shares the core with IDLE0 so the task watchdog stays fed
}

// Main-thread: dead-reckoning prediction, correction animation, trails, stale removal, distance/bearing.
void adsbLoop() {
  static uint32_t last = 0; uint32_t now = millis(); float dt = (now - last) / 1000.0f; last = now; if (dt > 1) dt = 1;
  adsbLock();
  for (int k = 0; k < MAX_TRACKS; k++) {
    Track& t = tracks[k]; if (!t.valid) continue;
    if (now - t.tFix > S.staleMs) { t.valid = false; continue; }
    if (t.corrActive) {
      float p = (now - t.corrStart) / (float)S.corrDurMs; if (p >= 1) { p = 1; t.corrActive = false; }
      p = p * p * (3 - 2 * p);
      t.dLat = t.corrFromLat + (t.lat - t.corrFromLat) * p; t.dLon = t.corrFromLon + (t.lon - t.corrFromLon) * p;
    } else if (S.prediction && !t.onGround && t.speedMs >= S.lowSpeed && now - t.tFix < S.predMaxMs) {
      float dKm = t.speedMs * dt / 1000.0f;
      t.dLat += cosf(t.headingDeg * DEG_TO_RAD) * dKm / 110.57f;
      t.dLon += sinf(t.headingDeg * DEG_TO_RAD) * dKm / (111.32f * cosf(S.lat * DEG_TO_RAD));
    }
    t.distKm = kmBetween(S.lat, S.lon, t.dLat, t.dLon); t.bearingDeg = bearingTo(S.lat, S.lon, t.dLat, t.dLon);
    if (now - t.trLast > 2000) { t.trLast = now; t.trLat[t.trHead] = t.dLat; t.trLon[t.trHead] = t.dLon; t.trHead = (t.trHead + 1) % TRAIL_LEN; if (t.trCount < TRAIL_LEN) t.trCount++; }
  }
  adsbUnlock();
}
