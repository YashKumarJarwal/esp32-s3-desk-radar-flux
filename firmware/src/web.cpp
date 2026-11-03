#include "web.h"
#include "display.h"
#include "settings.h"
#include "net.h"
#include "adsb.h"
#include "modes.h"
#include "theme.h"
#include "sky.h"
#include "pulse.h"
#include "web_index.h"
#include <WebServer.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <mbedtls/base64.h>

static WebServer server(80);
static bool authOk() {
  if (!S.pulseToken.length()) return true;
  return server.hasHeader("X-Token") && server.header("X-Token") == S.pulseToken;
}
static void json(int code, const String& s) { server.send(code, "application/json", s); }
static void ok() { json(200, "{\"ok\":true}"); }

static void handleIndex() {
  if (netState() == NetState::PortalAP && server.hostHeader() != WiFi.softAPIP().toString()) {   // captive portal redirect
    server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true); server.send(302, "text/plain", ""); return;
  }
  server.sendHeader("Content-Encoding", "gzip"); server.sendHeader("Cache-Control", "no-cache");
  server.send_P(200, "text/html", (const char*)WEB_INDEX_GZ, WEB_INDEX_LEN);
}
static void handleScreen() {
  const uint16_t* fb = frameBuffer(); if (!fb) { server.send(503, "text/plain", "no framebuffer"); return; }
  server.setContentLength(TFT_W * TFT_H * 2); server.send(200, "application/octet-stream", "");
  WiFiClient c = server.client(); const uint8_t* p = (const uint8_t*)fb; size_t left = TFT_W * TFT_H * 2;
  while (left) { size_t n = left > 4096 ? 4096 : left; size_t w = c.write(p, n); if (!w) break; p += w; left -= w; }
}
static void handleStatus() {
  const AdsbStats& a = adsbStats(); const esp_partition_t* run = esp_ota_get_running_partition();
  String j = "{\"version\":\"" FLUX_VERSION "\",\"build\":\"" __DATE__ " " __TIME__ "\",\"chip\":\"" + String(ESP.getChipModel()) + " r" + String(ESP.getChipRevision()) + "\",";
  j += "\"ip\":\"" + netIP() + "\",\"rssi\":" + String(netRSSI()) + ",\"heap\":" + String(ESP.getFreeHeap()) + ",\"psram\":" + String(ESP.getFreePsram()) + ",";
  j += "\"uptime\":" + String(millis() / 1000) + ",\"state\":" + String((int)netState()) + ",\"slot\":\"" + String(run ? run->label : "?") + "\",";
  j += "\"mode\":" + String(modeCurrent()) + ",\"modeName\":\"" + String(modeName(modeCurrent())) + "\",\"themeName\":\"" + String(themeGet(S.theme).name) + "\",\"rangeKm\":" + String((int)S.rangeKm) + ",";
  j += "\"modes\":["; for (int i = 0; i < MODE_COUNT; i++) { j += String(i ? "," : "") + "\"" + modeName(i) + "\""; } j += "],";
  j += "\"api\":{\"provider\":\"" + String(a.provider) + "\",\"status\":" + String((int)a.status) + ",\"http\":" + String(a.httpCode) + ",\"active\":" + String(a.active) + ",\"interval\":" + String(a.intervalMs) + ",\"today\":" + String(a.requestsToday) + ",\"dur\":" + String(a.durMs) + ",\"err\":\"" + String(a.err) + "\"},";
  j += "\"cards\":" + pulseListJson() + "}";
  json(200, j);
}
static void handleAircraft() {
  String j = "{\"aircraft\":["; bool first = true; adsbLock(); Track* t = adsbTracks();
  for (int k = 0; k < MAX_TRACKS; k++) if (t[k].valid) {
    j += String(first ? "" : ",") + "{\"cs\":\"" + String(t[k].callsign) + "\",\"t\":\"" + String(t[k].type) + "\",\"reg\":\"" + String(t[k].reg) + "\",\"desc\":\"" + String(t[k].desc) + "\",\"alt\":" + String((int)t[k].altM) + ",\"spd\":" + String((int)(t[k].speedMs * 3.6f)) + ",\"hdg\":" + String((int)t[k].headingDeg) + ",\"dist\":" + String(t[k].distKm, 1) + ",\"brg\":" + String((int)t[k].bearingDeg) + ",\"lat\":" + String(t[k].dLat, 5) + ",\"lon\":" + String(t[k].dLon, 5) + ",\"ground\":" + String(t[k].onGround ? "true" : "false") + "}";
    first = false;
  }
  adsbUnlock(); json(200, j + "]}");
}
static void handleGetSettings() { json(200, settingsToJson(false)); }
static void handlePostSettings() {
  String err; if (!settingsFromJson(server.arg("plain"), err)) { json(400, "{\"ok\":false,\"err\":\"" + err + "\"}"); return; }
  settingsSave(); adsbRequestNow(); ok();
}
static void handleAction() {
  JsonDocument d; deserializeJson(d, server.arg("plain")); String a = d["action"] | "";
  if (a == "theme") skyMode.nextTheme(); else if (a == "range") skyMode.nextRange(); else if (a == "refresh") adsbRequestNow();
  else if (a == "qr") overlayQR(!overlayQRActive()); else if (a == "mode") modeSwitch(d["value"].as<int>());
  else if (a == "short") inputInject(Btn::Short); else if (a == "double") inputInject(Btn::Double); else if (a == "long") inputInject(Btn::Long); else if (a == "triple") inputInject(Btn::Triple);
  else { json(400, "{\"ok\":false,\"err\":\"unknown action\"}"); return; }
  ok();
}
static void handleScan() {
  int n = WiFi.scanNetworks(false, false, false, 200); String j = "{\"networks\":[";
  for (int i = 0; i < n; i++) { String ss = WiFi.SSID(i); ss.replace("\"", "'"); j += String(i ? "," : "") + "{\"ssid\":\"" + ss + "\",\"rssi\":" + String(WiFi.RSSI(i)) + ",\"open\":" + String(WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "true" : "false") + "}"; }
  WiFi.scanDelete(); json(200, j + "]}");
}
static void handleRestart() { ok(); delay(200); ESP.restart(); }
static void handleReset() { settingsReset(); settingsSave(); ok(); delay(200); ESP.restart(); }
static void handleRollback() {
  if (Update.canRollBack()) { ok(); delay(200); Update.rollBack(); ESP.restart(); } else json(400, "{\"ok\":false,\"err\":\"no previous firmware\"}");
}
static void handleOtaDone() {
  adsbPause(false);
  if (Update.hasError()) { json(500, "{\"ok\":false,\"err\":\"" + String(Update.errorString()) + "\"}"); return; }
  ok(); delay(300); ESP.restart();
}
static void handleOtaUpload() {
  HTTPUpload& up = server.upload();
  if (up.status == UPLOAD_FILE_START) { adsbPause(true); Serial.printf("[ota] start %s\n", up.filename.c_str()); if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial); }
  else if (up.status == UPLOAD_FILE_WRITE) { if (Update.write(up.buf, up.currentSize) != up.currentSize) Update.printError(Serial); }
  else if (up.status == UPLOAD_FILE_END) { if (Update.end(true)) Serial.printf("[ota] done %u bytes\n", up.totalSize); else Update.printError(Serial); }
}
static void handlePulse() {
  if (!authOk()) { json(401, "{\"ok\":false,\"err\":\"token\"}"); return; }
  String err; if (!pulseUpsert(server.arg("plain"), err)) { json(400, "{\"ok\":false,\"err\":\"" + err + "\"}"); return; } ok();
}
static void handlePulseDelete() { if (!authOk()) { json(401, "{}"); return; } pulseRemove(server.arg("id").c_str()); ok(); }
static void handlePulseImage() {
  if (!authOk()) { json(401, "{}"); return; }
  // body: JSON {"id":"...","b64":"<base64 of 8192 bytes RGB565 little-endian>"} (raw binary would be truncated at NUL bytes by WebServer)
  JsonDocument d; if (deserializeJson(d, server.arg("plain"))) { json(400, "{\"ok\":false,\"err\":\"json\"}"); return; }
  const char* id = d["id"] | ""; const char* b64 = d["b64"] | "";
  static uint8_t buf[64 * 64 * 2]; size_t outLen = 0;
  if (mbedtls_base64_decode(buf, sizeof buf, &outLen, (const uint8_t*)b64, strlen(b64)) != 0 || outLen != sizeof buf) { json(400, "{\"ok\":false,\"err\":\"need base64 of exactly 8192 bytes\"}"); return; }
  if (!pulseSetImage(id, buf, outLen)) { json(400, "{\"ok\":false,\"err\":\"unknown card id\"}"); return; } ok();
}

void webBegin() {
  const char* hdrs[] = {"X-Token", "Host"}; server.collectHeaders(hdrs, 2);
  server.on("/", HTTP_GET, handleIndex);
  server.on("/generate_204", HTTP_GET, handleIndex); server.on("/hotspot-detect.html", HTTP_GET, handleIndex); server.on("/connecttest.txt", HTTP_GET, handleIndex);
  server.on("/favicon.ico", HTTP_GET, [] { server.send(204, "image/x-icon", ""); });
  server.on("/screen.raw", HTTP_GET, handleScreen);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/aircraft", HTTP_GET, handleAircraft);
  server.on("/api/settings", HTTP_GET, handleGetSettings);
  server.on("/api/settings", HTTP_POST, handlePostSettings);
  server.on("/api/action", HTTP_POST, handleAction);
  server.on("/api/wifi/scan", HTTP_GET, handleScan);
  server.on("/api/restart", HTTP_POST, handleRestart);
  server.on("/api/reset", HTTP_POST, handleReset);
  server.on("/api/rollback", HTTP_POST, handleRollback);
  server.on("/api/ota", HTTP_POST, handleOtaDone, handleOtaUpload);
  server.on("/api/pulse", HTTP_POST, handlePulse);
  server.on("/api/pulse", HTTP_DELETE, handlePulseDelete);
  server.on("/api/pulse/image", HTTP_POST, handlePulseImage);
  server.onNotFound([] { if (netState() == NetState::PortalAP) handleIndex(); else server.send(404, "text/plain", "FLUX " FLUX_VERSION); });
  server.begin();
}
void webLoop() { server.handleClient(); }
