#include "auxdata.h"
#include "settings.h"
#include "net.h"
#include "modes.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static SemaphoreHandle_t httpsMtx;
static Weather W = {}; static IssFix I = {};
static double issTrail[ISS_TRAIL * 2]; static int trailCount = 0, trailHead = 0;
#define ROUTE_CACHE 8
static RouteInfo routes[ROUTE_CACHE]; static char pendingRoute[9] = "";

void httpsLock() { xSemaphoreTake(httpsMtx, portMAX_DELAY); }
void httpsUnlock() { xSemaphoreGive(httpsMtx); }
const Weather& auxWeather() { return W; }
const IssFix& auxIss() { return I; }
const double* auxIssTrail(int& c, int& h) { c = trailCount; h = trailHead; return issTrail; }

static bool getJson(const String& url, JsonDocument& doc, JsonDocument* filter = nullptr) {
  if (!netOnline()) return false;
  httpsLock();
  WiFiClientSecure c; c.setInsecure(); c.setTimeout(8000); HTTPClient http; http.setTimeout(9000); http.setUserAgent("FLUX/" FLUX_VERSION); http.setReuse(false); http.useHTTP10(true);
  bool ok = false;
  if (http.begin(c, url)) {
    http.addHeader("Connection", "close");
    int code = http.GET();
    if (code == 200) { String body = http.getString(); ok = !(filter ? deserializeJson(doc, body, DeserializationOption::Filter(*filter)) : deserializeJson(doc, body)); }
    else if (S.serialDebug) Serial.printf("[aux] %s -> HTTP %d\n", url.c_str(), code);
    http.end();
  }
  httpsUnlock(); return ok;
}

static void fetchWeather() {
  JsonDocument d;
  String u = "https://api.open-meteo.com/v1/forecast?latitude=" + String(S.lat, 4) + "&longitude=" + String(S.lon, 4) +
    "&current=temperature_2m,relative_humidity_2m,apparent_temperature,weather_code,wind_speed_10m,is_day&hourly=temperature_2m,precipitation_probability&forecast_days=1&timezone=UTC";
  if (!getJson(u, d)) return;
  JsonObject c = d["current"]; W.tempC = c["temperature_2m"] | 0.0f; W.feelsC = c["apparent_temperature"] | 0.0f; W.humidity = c["relative_humidity_2m"] | 0.0f;
  W.windKmh = c["wind_speed_10m"] | 0.0f; W.code = c["weather_code"] | 0; W.isDay = c["is_day"] | 1;
  JsonArray ht = d["hourly"]["temperature_2m"], hp = d["hourly"]["precipitation_probability"]; W.hours = 0;
  for (size_t i = 0; i < ht.size() && i < 24; i++) { W.hourT[i] = ht[i] | 0.0f; W.hourP[i] = hp[i] | 0; W.hours++; }
  JsonDocument a;
  if (getJson("https://air-quality-api.open-meteo.com/v1/air-quality?latitude=" + String(S.lat, 4) + "&longitude=" + String(S.lon, 4) + "&current=us_aqi,pm2_5,pm10", a)) {
    W.aqi = a["current"]["us_aqi"] | 0; W.pm25 = a["current"]["pm2_5"] | 0.0f; W.pm10 = a["current"]["pm10"] | 0.0f;
  }
  W.valid = true; W.at = millis();
  Serial.printf("[aux] weather %.1fC code=%d aqi=%d\n", W.tempC, W.code, W.aqi);
}

static void fetchIss() {
  JsonDocument d;
  if (!getJson("https://api.wheretheiss.at/v1/satellites/25544", d)) return;
  I.lat = d["latitude"] | 0.0; I.lon = d["longitude"] | 0.0; I.altKm = d["altitude"] | 0.0f; I.velKmh = d["velocity"] | 0.0f;
  strlcpy(I.visibility, d["visibility"] | "", sizeof I.visibility);
  // great-circle distance / bearing / elevation from the observer
  double la1 = S.lat * DEG_TO_RAD, lo1 = S.lon * DEG_TO_RAD, la2 = I.lat * DEG_TO_RAD, lo2 = I.lon * DEG_TO_RAD;
  double dlon = lo2 - lo1; double cosc = sin(la1) * sin(la2) + cos(la1) * cos(la2) * cos(dlon); double cang = acos(constrain(cosc, -1.0, 1.0));
  I.distKm = cang * 6371.0;
  I.bearing = fmod(atan2(sin(dlon) * cos(la2), cos(la1) * sin(la2) - sin(la1) * cos(la2) * cos(dlon)) * RAD_TO_DEG + 360.0, 360.0);
  double Re = 6371.0, h = I.altKm; I.elevDeg = atan2(cos(cang) - Re / (Re + h), sin(cang)) * RAD_TO_DEG;
  I.valid = true; I.at = millis();
  issTrail[trailHead * 2] = I.lat; issTrail[trailHead * 2 + 1] = I.lon; trailHead = (trailHead + 1) % ISS_TRAIL; if (trailCount < ISS_TRAIL) trailCount++;
}

const RouteInfo* auxRoute(const char* cs) {
  if (!cs || !cs[0]) return nullptr;
  for (auto& r : routes) if (r.valid && !strcmp(r.callsign, cs)) return &r;
  if (!pendingRoute[0]) strlcpy(pendingRoute, cs, sizeof pendingRoute);
  return nullptr;
}
// fonts are ASCII only: fold common accented letters, drop the rest
static void asciiFold(char* s) {
  char out[32]; int o = 0;
  for (unsigned char* p = (unsigned char*)s; *p && o < (int)sizeof(out) - 1; p++) {
    if (*p < 0x80) { out[o++] = *p; continue; }
    if (*p == 0xC3 && p[1]) { unsigned char c = p[1]; p++; const char* m = (c >= 0xA0 && c <= 0xA5) ? "a" : (c >= 0xA8 && c <= 0xAB) ? "e" : (c >= 0xAC && c <= 0xAF) ? "i" : (c >= 0xB2 && c <= 0xB6) ? "o" : (c >= 0xB9 && c <= 0xBC) ? "u" : c == 0xA7 ? "c" : c == 0xB1 ? "n" : (c >= 0x80 && c <= 0x85) ? "A" : (c >= 0x88 && c <= 0x8B) ? "E" : (c >= 0x92 && c <= 0x96) ? "O" : (c >= 0x99 && c <= 0x9C) ? "U" : ""; if (*m) out[o++] = *m; continue; }
    if (*p == 0xC4 || *p == 0xC5) { unsigned char c = p[1]; p++; const char* m = (*p == 0xC5 && (c == 0x9F || c == 0x9E)) ? "s" : (*p == 0xC4 && (c == 0x9F || c == 0x9E)) ? "g" : (*p == 0xC4 && c == 0xB1) ? "i" : ""; if (*m) out[o++] = *m; continue; }
    if (*p >= 0xC0) { int extra = *p >= 0xF0 ? 3 : *p >= 0xE0 ? 2 : 1; p += extra; }
  }
  out[o] = 0; strcpy(s, out);
}
static void fetchRoute() {
  char cs[9]; strlcpy(cs, pendingRoute, sizeof cs);
  int slot = 0; uint32_t oldest = UINT32_MAX; for (int i = 0; i < ROUTE_CACHE; i++) if (!routes[i].valid) { slot = i; break; } else if (routes[i].at < oldest) { oldest = routes[i].at; slot = i; }
  RouteInfo& r = routes[slot]; memset(&r, 0, sizeof r); strlcpy(r.callsign, cs, sizeof r.callsign); r.valid = true; r.at = millis();  // mark even on failure (avoid re-querying)
  JsonDocument d;
  if (getJson("https://api.adsbdb.com/v0/callsign/" + String(cs), d)) {
    JsonObject fr = d["response"]["flightroute"];
    strlcpy(r.airline, fr["airline"]["name"] | "", sizeof r.airline); strlcpy(r.from, fr["origin"]["iata_code"] | "", sizeof r.from); strlcpy(r.to, fr["destination"]["iata_code"] | "", sizeof r.to);
    strlcpy(r.fromCity, fr["origin"]["municipality"] | "", sizeof r.fromCity); strlcpy(r.toCity, fr["destination"]["municipality"] | "", sizeof r.toCity);
    asciiFold(r.airline); asciiFold(r.fromCity); asciiFold(r.toCity);
    r.fromLat = fr["origin"]["latitude"] | 0.0; r.fromLon = fr["origin"]["longitude"] | 0.0; r.toLat = fr["destination"]["latitude"] | 0.0; r.toLon = fr["destination"]["longitude"] | 0.0;
    if (S.serialDebug) Serial.printf("[aux] route %s: %s %s-%s\n", cs, r.airline, r.from, r.to);
  }
  pendingRoute[0] = 0;
}

static void task(void*) {
  uint32_t tW = 0, tI = 0;
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(500));
    if (!netOnline()) continue;
    uint32_t now = millis();
    bool wantW = ((S.modesEnabled >> MODE_ATMOS) & 1), wantI = ((S.modesEnabled >> MODE_ORBIT) & 1);
    if (wantW && (now - tW > 15 * 60000UL || !W.valid) && (tW == 0 || now - tW > 60000)) { tW = now; fetchWeather(); }
    if (wantI && now - tI > 10000) { tI = now; fetchIss(); }
    if (pendingRoute[0]) fetchRoute();
  }
}
void auxBegin() { httpsMtx = xSemaphoreCreateMutex(); xTaskCreatePinnedToCore(task, "aux", 10240, nullptr, 0, nullptr, 0); }
