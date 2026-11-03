#include "sigradar.h"
#include "display.h"
#include "settings.h"
#include "theme.h"
#include <NimBLEDevice.h>
#include <WiFi.h>
SignalMode signalMode;
#define MAX_DEV 40
struct Dev { uint8_t addr[6]; int8_t rssi; char name[14]; uint32_t seen; bool valid; };
static Dev devs[MAX_DEV]; static float anim = 0; static bool bleOn = false; static uint32_t tWifi = 0, tBle = 0; static int wifiN = 0; static int layer = 0;
struct WNet { char ssid[20]; int8_t rssi; uint32_t hash; }; static WNet wn[20];

class CB : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* d) override {
    const uint8_t* a = d->getAddress().getBase()->val; Dev* slot = nullptr;
    for (auto& x : devs) { if (x.valid && !memcmp(x.addr, a, 6)) { slot = &x; break; } }
    if (!slot) { for (auto& x : devs) if (!x.valid) { slot = &x; break; } }
    if (!slot) { uint32_t o = UINT32_MAX; for (auto& x : devs) if (x.seen < o) { o = x.seen; slot = &x; } }
    memcpy(slot->addr, a, 6); slot->rssi = d->getRSSI(); slot->seen = millis(); slot->valid = true;
    if (d->haveName()) strlcpy(slot->name, d->getName().c_str(), sizeof slot->name); else if (!slot->name[0]) slot->name[0] = 0;
  }
} cb;

void SignalMode::enter() {
  if (!bleOn) { NimBLEDevice::init("flux"); bleOn = true; }
  NimBLEScan* s = NimBLEDevice::getScan(); s->setScanCallbacks(&cb, false); s->setActiveScan(true); s->setInterval(160); s->setWindow(48); s->setDuplicateFilter(true); s->setMaxResults(0);
  s->start(4000, false, true);   // 4 s burst; update() re-arms every 15 s so WiFi gets breathing room
  tWifi = 0; tBle = millis();
}
void SignalMode::exit() { NimBLEDevice::getScan()->stop(); NimBLEDevice::deinit(true); bleOn = false; for (auto& x : devs) x.valid = false; }  // release ~60 KB heap when leaving the mode
bool SignalMode::onButton(Btn b) { if (b == Btn::Short) { layer = (layer + 1) % 3; return true; } return false; }
void SignalMode::update(uint32_t now, float dt) {
  anim += dt;
  for (auto& x : devs) if (x.valid && now - x.seen > 45000) x.valid = false;
  NimBLEScan* sc = NimBLEDevice::getScan();
  if (now - tBle > 15000 && !sc->isScanning()) { tBle = now; sc->clearResults(); sc->start(4000, false, true); }
  if (now - tWifi > 90000 && !sc->isScanning()) {
    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_FAILED || (n >= 0 && tWifi)) { if (n >= 0) { wifiN = min(n, 20); for (int i = 0; i < wifiN; i++) { strlcpy(wn[i].ssid, WiFi.SSID(i).c_str(), sizeof wn[i].ssid); wn[i].rssi = WiFi.RSSI(i); uint32_t h = 2166136261u; for (char* p = wn[i].ssid; *p; p++) h = (h ^ *p) * 16777619u; wn[i].hash = h; } WiFi.scanDelete(); } tWifi = now; WiFi.scanNetworks(true, false, false, 120); }
    else if (!tWifi) { tWifi = now; WiFi.scanNetworks(true, false, false, 120); }
  }
}
static float rssiToR(int rssi) { float d = powf(10.0f, (-59 - rssi) / 25.0f); return constrain(d / 12.0f, 0.06f, 1.0f); } // ~0..12 m mapped to 0..1
void SignalMode::draw() {
  const Theme& T = themeGet(S.theme); frame.fillSprite(T.bg); const int CX = 120, CY = 120, R = 100;
  for (int i = 1; i <= 3; i++) frame.drawCircle(CX, CY, R * i / 3, i == 3 ? T.ring : T.ringDim);
  float sw = fmodf(anim * 60, 360); for (int i = 6; i >= 1; i--) frame.fillArc(CX, CY, 0, R, sw - i * 4, sw - (i - 1) * 4, cLerp(T.bg, T.sweepDim, 255 * (7 - i) / 7));
  frame.fillCircle(CX, CY, 3, T.accent);
  frame.setTextDatum(textdatum_t::middle_center); frame.setFont(&fonts::Font0);
  int nb = 0;
  if (layer != 2) for (auto& x : devs) if (x.valid) {
    uint32_t h = (x.addr[3] << 16) | (x.addr[4] << 8) | x.addr[5]; float a = (h % 360) * DEG_TO_RAD; float r = rssiToR(x.rssi) * R;
    float px = CX + cosf(a) * r, py = CY + sinf(a) * r; uint8_t age = constrain(255 - (int)((millis() - x.seen) / 120), 60, 255);
    uint16_t col = cLerp(T.bg, x.name[0] ? T.aircraftAlt : T.aircraft, age);
    frame.fillCircle(px, py, x.name[0] ? 3 : 2, col); frame.drawCircle(px, py, 5, cLerp(T.bg, col, 90));
    if (x.name[0] && layer == 0) { frame.setTextColor(cLerp(T.bg, T.label, age)); frame.drawString(x.name, px, py - 9); }
    nb++;
  }
  if (layer != 1) for (int i = 0; i < wifiN; i++) { float a0 = wn[i].hash % 360; float w = constrain((wn[i].rssi + 100) / 4.0f, 3.0f, 18.0f); uint16_t c = cLerp(T.bg, T.vector, constrain((wn[i].rssi + 95) * 4, 60, 255)); frame.fillArc(CX, CY, R + 4, R + 9, a0 - w / 2, a0 + w / 2, c); if (layer == 2 || wn[i].rssi > -60) { float am = a0 * DEG_TO_RAD; frame.setTextColor(c); frame.drawString(wn[i].ssid, CX + cosf(am) * (R - 14), CY + sinf(am) * (R - 14)); } }
  frame.setFont(&fonts::Font2); frame.setTextColor(T.text); frame.drawString("SIGNAL", CX, 16);
  frame.setFont(&fonts::Font0); frame.setTextColor(T.labelDim);
  char b[40]; snprintf(b, sizeof b, "%d BLE   %d WIFI   %s", nb, wifiN, layer == 0 ? "ALL" : layer == 1 ? "BLE" : "WIFI"); frame.drawString(b, CX, 226);
}
