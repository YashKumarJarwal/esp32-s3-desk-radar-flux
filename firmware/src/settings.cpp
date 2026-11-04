#include "settings.h"
#include <Preferences.h>
#include <ArduinoJson.h>

Settings S;
static Preferences prefs;
static const char* NS = "flux";

static float gf(Preferences& p, const char* k, float d) { return p.isKey(k) ? p.getFloat(k, d) : d; }

// Import the stock firmware's settings (namespace radar_cfg) so nothing the user configured is lost.
static void importStock() {
  Preferences st;
  if (!st.begin("radar_cfg", true)) { Serial.println("[settings] no stock namespace"); return; }
  if (!st.isKey("ver")) { st.end(); return; }
  Serial.println("[settings] importing stock radar_cfg settings");
  S.ssid = st.getString("ssid", ""); S.wpass = st.getString("wpass", "");
  S.configured = st.getUChar("wcfg", 0) && S.ssid.length();
  S.lat = gf(st, "clat", S.lat); S.lon = gf(st, "clon", S.lon);
  S.rangeKm = gf(st, "range", S.rangeKm); S.fetchKm = gf(st, "fetchKm", S.fetchKm);
  S.preset[0] = gf(st, "rng0", 30); S.preset[1] = gf(st, "rng1", 60); S.preset[2] = gf(st, "rng2", 120);
  S.edgeDots = st.getUChar("edgeDots", 1);
  S.theme = st.getUChar("ui", 0) % 3; S.maxAircraft = st.getUChar("maxac", 12);
  S.labels = st.getUChar("labels", 1); S.brightness = st.getUChar("bright", 255);
  S.provider = (Provider)st.getUChar("prov", 1) == Provider::OpenSky ? Provider::OpenSky : Provider::AdsbFi;
  // stock: prov 1 == adsb.fi (boot log "api=adsb.fi" with prov=1)
  S.provider = st.getUChar("prov", 1) == 1 ? Provider::AdsbFi : Provider::OpenSky;
  S.account = st.getUChar("acct", 0); S.refreshPolicy = st.getUChar("rpol", 1);
  S.dailyBudget = st.getUInt("budget", 400); S.reserve = gf(st, "reserve", 0.9f); S.cost = gf(st, "cost", 1.0f);
  S.manualIntervalMs = st.getUInt("manint", 10000); S.minIntervalMs = st.getUInt("minint", 1000);
  S.osUser = st.getString("osuser", ""); S.osPass = st.getString("ospass", "");
  S.osClientId = st.getString("oscid", ""); S.osClientSecret = st.getString("oscsec", "");
  S.scheduleEnabled = st.getUChar("sch", 0); S.startMin = st.getShort("start", 480); S.endMin = st.getShort("end", 1200);
  S.tzMin = st.getShort("tz", 330); S.idle = (IdleMode)st.getUChar("idle", 0);
  S.showGround = st.getUChar("ground", 0); S.minAltM = gf(st, "minalt", 100); S.minSpeedMs = gf(st, "minspd", 30);
  S.prediction = st.getUChar("pred", 1); S.alpha = gf(st, "alpha", 0.15f); S.predMaxMs = st.getUInt("pmax", 120000);
  S.jumpKm = gf(st, "jump", 20); S.lowSpeed = gf(st, "lowspd", 20); S.staleMs = st.getUInt("stale", 120000);
  S.correction = st.getUChar("corr", 1); S.corrMinApiMs = st.getUInt("cmin", 30000); S.corrDurMs = st.getUInt("cdur", 6000);
  S.corrStartKm = gf(st, "cdist", 0.5f);
  S.buttonPin = (int8_t)st.getShort("btn", -1); S.serialDebug = st.getUChar("dbg", 0); S.txPowerRaw = st.getUChar("txpwr", 76);
  st.end();
}

#define J_SET(doc, name, field) doc[name] = S.field
#define J_GET(doc, name, field, T) if (doc[name].is<T>()) S.field = doc[name].as<T>()

String settingsToJson(bool secrets) {
  JsonDocument d;
  d["configured"] = S.configured; d["ssid"] = S.ssid; d["wpass"] = secrets ? S.wpass : (S.wpass.length() ? "********" : "");
  d["hostname"] = S.hostname; d["lat"] = S.lat; d["lon"] = S.lon; d["rangeKm"] = S.rangeKm;
  JsonArray p = d["preset"].to<JsonArray>(); for (int i = 0; i < 3; i++) p.add(S.preset[i]);
  d["fetchKm"] = S.fetchKm; d["edgeDots"] = S.edgeDots; d["edgeExtKm"] = S.edgeExtKm;
  d["theme"] = S.theme; d["maxAircraft"] = S.maxAircraft; d["labels"] = S.labels; d["brightness"] = S.brightness;
  d["provider"] = (int)S.provider; d["account"] = S.account; d["refreshPolicy"] = S.refreshPolicy; d["dailyBudget"] = S.dailyBudget;
  d["reserve"] = S.reserve; d["cost"] = S.cost; d["manualIntervalMs"] = S.manualIntervalMs; d["minIntervalMs"] = S.minIntervalMs;
  d["osUser"] = S.osUser; d["osPass"] = secrets ? S.osPass : (S.osPass.length() ? "********" : "");
  d["osClientId"] = S.osClientId; d["osClientSecret"] = secrets ? S.osClientSecret : (S.osClientSecret.length() ? "********" : "");
  d["scheduleEnabled"] = S.scheduleEnabled; d["startMin"] = S.startMin; d["endMin"] = S.endMin; d["tzMin"] = S.tzMin; d["idle"] = (int)S.idle;
  d["showGround"] = S.showGround; d["minAltM"] = S.minAltM; d["minSpeedMs"] = S.minSpeedMs;
  d["prediction"] = S.prediction; d["alpha"] = S.alpha; d["predMaxMs"] = S.predMaxMs; d["jumpKm"] = S.jumpKm; d["lowSpeed"] = S.lowSpeed; d["staleMs"] = S.staleMs;
  d["correction"] = S.correction; d["corrMinApiMs"] = S.corrMinApiMs; d["corrDurMs"] = S.corrDurMs; d["corrStartKm"] = S.corrStartKm;
  d["buttonPin"] = S.buttonPin; d["serialDebug"] = S.serialDebug; d["txPowerRaw"] = S.txPowerRaw;
  d["lang"] = S.lang; d["mode"] = S.mode; d["modesEnabled"] = S.modesEnabled; d["trails"] = S.trails; d["lockOn"] = S.lockOn;
  d["overheadAlertKm"] = S.overheadAlertKm; d["autoDimNight"] = S.autoDimNight; d["nightBrightness"] = S.nightBrightness; d["pulseToken"] = S.pulseToken;
  String out; serializeJson(d, out); return out;
}

bool settingsFromJson(const String& json, String& err) {
  JsonDocument d; auto e = deserializeJson(d, json);
  if (e) { err = e.c_str(); return false; }
  auto str = [&](const char* k, String& f, bool secret) { if (d[k].is<const char*>()) { String v = d[k].as<String>(); if (!(secret && v == "********")) f = v; } };
  // never let a blank SSID/password from a form wipe saved credentials; clearing is only via factory reset
  { String v = d["ssid"].is<const char*>() ? d["ssid"].as<String>() : String(); if (v.length()) S.ssid = v; }
  { String v = d["wpass"].is<const char*>() ? d["wpass"].as<String>() : String(); if (v.length() && v != "********") S.wpass = v; }
  str("hostname", S.hostname, false);
  str("pulseToken", S.pulseToken, false); str("osUser", S.osUser, false); str("osPass", S.osPass, true); str("osClientId", S.osClientId, false); str("osClientSecret", S.osClientSecret, true);
  J_GET(d, "lat", lat, double); J_GET(d, "lon", lon, double); J_GET(d, "rangeKm", rangeKm, float);
  if (d["preset"].is<JsonArray>()) { int i = 0; for (JsonVariant v : d["preset"].as<JsonArray>()) { if (i < 3) S.preset[i++] = v.as<float>(); } }
  J_GET(d, "fetchKm", fetchKm, float); J_GET(d, "edgeDots", edgeDots, bool); J_GET(d, "edgeExtKm", edgeExtKm, float);
  J_GET(d, "theme", theme, uint8_t); J_GET(d, "maxAircraft", maxAircraft, uint8_t); J_GET(d, "labels", labels, bool); J_GET(d, "brightness", brightness, uint8_t);
  if (d["provider"].is<int>()) S.provider = (Provider)d["provider"].as<int>();
  J_GET(d, "account", account, uint8_t); J_GET(d, "refreshPolicy", refreshPolicy, uint8_t); J_GET(d, "dailyBudget", dailyBudget, uint32_t);
  J_GET(d, "reserve", reserve, float); J_GET(d, "cost", cost, float); J_GET(d, "manualIntervalMs", manualIntervalMs, uint32_t); J_GET(d, "minIntervalMs", minIntervalMs, uint32_t);
  J_GET(d, "scheduleEnabled", scheduleEnabled, bool); J_GET(d, "startMin", startMin, int16_t); J_GET(d, "endMin", endMin, int16_t); J_GET(d, "tzMin", tzMin, int16_t);
  if (d["idle"].is<int>()) S.idle = (IdleMode)d["idle"].as<int>();
  J_GET(d, "showGround", showGround, bool); J_GET(d, "minAltM", minAltM, float); J_GET(d, "minSpeedMs", minSpeedMs, float);
  J_GET(d, "prediction", prediction, bool); J_GET(d, "alpha", alpha, float); J_GET(d, "predMaxMs", predMaxMs, uint32_t); J_GET(d, "jumpKm", jumpKm, float);
  J_GET(d, "lowSpeed", lowSpeed, float); J_GET(d, "staleMs", staleMs, uint32_t); J_GET(d, "correction", correction, bool);
  J_GET(d, "corrMinApiMs", corrMinApiMs, uint32_t); J_GET(d, "corrDurMs", corrDurMs, uint32_t); J_GET(d, "corrStartKm", corrStartKm, float);
  J_GET(d, "buttonPin", buttonPin, int8_t); J_GET(d, "serialDebug", serialDebug, bool); J_GET(d, "txPowerRaw", txPowerRaw, uint8_t);
  J_GET(d, "lang", lang, uint8_t); J_GET(d, "mode", mode, uint8_t); J_GET(d, "modesEnabled", modesEnabled, uint16_t); J_GET(d, "trails", trails, bool);
  J_GET(d, "lockOn", lockOn, bool); J_GET(d, "overheadAlertKm", overheadAlertKm, float); J_GET(d, "autoDimNight", autoDimNight, bool); J_GET(d, "nightBrightness", nightBrightness, uint8_t);
  if (S.ssid.length()) S.configured = true;
  // sanity clamps
  S.rangeKm = constrain(S.rangeKm, 5.0f, 300.0f);
  S.fetchKm = constrain(S.fetchKm, S.rangeKm, min(S.rangeKm * 1.5f, 300.0f));
  S.maxAircraft = constrain(S.maxAircraft, (uint8_t)1, (uint8_t)40);
  S.manualIntervalMs = max(S.manualIntervalMs, S.minIntervalMs);
  return true;
}

void settingsBegin() {
  prefs.begin(NS, false);
  if (prefs.isKey("json")) {
    String err; String j = prefs.getString("json", "");
    if (!settingsFromJson(j, err)) Serial.printf("[settings] load error: %s\n", err.c_str());
    else Serial.println("[settings] loaded orbit settings");
  } else {
    importStock();
    settingsSave();
  }
  Serial.printf("[settings] configured=%d ssid_set=%d centre=%.5f,%.5f range=%.0f presets=%.0f/%.0f/%.0f theme=%u bright=%u\n",
    S.configured, S.ssid.length() > 0, S.lat, S.lon, S.rangeKm, S.preset[0], S.preset[1], S.preset[2], S.theme, S.brightness);
}
bool settingsSave() { return prefs.putString("json", settingsToJson(true)) > 0; }
void settingsReset() { prefs.clear(); S = Settings(); }
