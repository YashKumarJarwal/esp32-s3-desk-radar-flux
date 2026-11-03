#include "modes.h"
#include "sky.h"
#include "pulse_ui.h"
#include "modes_extra.h"
#include "sigradar.h"
#include "flight.h"
#include "geo.h"
#include "pulse.h"
#include "display.h"
#include "settings.h"
#include "theme.h"
#include "net.h"
#include "clock.h"
#include "adsb.h"
#include <qrcode.h>

static Mode* modes[MODE_COUNT] = {nullptr};
static uint8_t cur = MODE_SKY, nextMode = 255; static uint32_t transStart = 0;
static bool qrOn = false; static String toast; static uint32_t toastUntil = 0;
static uint32_t lastTick = 0;
static const char* NAMES[MODE_COUNT] = {"SKY", "FLIGHT", "CHRONO", "ATMOS", "ORBIT", "SIGNAL", "PULSE"};
const char* modeName(uint8_t id) { return id < MODE_COUNT ? NAMES[id] : "?"; }
uint8_t modeCurrent() { return cur; }
void overlayQR(bool on) { qrOn = on; }
bool overlayQRActive() { return qrOn; }
void overlayToast(const String& s, uint32_t ms) { toast = s; toastUntil = millis() + ms; }

void modesBegin() {
  modes[MODE_SKY] = &skyMode;
  modes[MODE_PULSE] = &pulseMode;
  modes[MODE_FLIGHT] = &flightMode; modes[MODE_CHRONO] = &chronoMode; modes[MODE_ATMOS] = &atmosMode; modes[MODE_ORBIT] = &orbitMode; modes[MODE_SIGNAL] = &signalMode;
  cur = S.mode < MODE_COUNT && modes[S.mode] ? S.mode : MODE_SKY;
  modes[cur]->enter();
}
void modeSwitch(uint8_t id) {
  if (id >= MODE_COUNT || !modes[id] || id == cur) return;
  nextMode = id; transStart = millis();
}

void drawConnectScreen() {
  const Theme& T = themeGet(S.theme);
  static float ph = 0; ph += 0.07f;
  frame.fillSprite(T.bg);
  for (int r = 30; r <= 104; r += 37) frame.drawCircle(120, 120, r, T.ringDim);
  for (int i = 0; i < 30; i++) { float a = ph - i * 0.03f; frame.drawLine(120, 120, 120 + cosf(a) * 104, 120 + sinf(a) * 104, cLerp(T.bg, T.sweep, 255 - i * 8)); }
  frame.setFont(&fonts::Orbitron_Light_24); frame.setTextDatum(textdatum_t::middle_center); frame.setTextColor(T.accent);
  frame.drawString("FLUX", 120, 100);
  frame.setFont(&fonts::Font2); frame.setTextColor(T.text);
  String s;
  switch (netState()) {
    case NetState::Connecting: s = "LINKING " + S.ssid; break;
    case NetState::PortalAP: s = "SETUP  " + netIP(); break;
    case NetState::Connected: s = netIP(); break;
    default: s = "BOOT"; break;
  }
  frame.drawString(s, 120, 134);
  frame.setTextColor(T.labelDim); frame.drawString(FLUX_VERSION, 120, 154);
}

static void drawQR() {
  const Theme& T = themeGet(S.theme);
  String url = "http://" + netIP() + "/";
  QRCode qr; uint8_t buf[qrcode_getBufferSize(3)];
  qrcode_initText(&qr, buf, 3, ECC_LOW, url.c_str());
  int sc = 4, sz = qr.size * sc, ox = 120 - sz / 2, oy = 120 - sz / 2;
  frame.fillRoundRect(ox - 8, oy - 8, sz + 16, sz + 16, 6, TFT_WHITE);
  for (int y = 0; y < qr.size; y++) for (int x = 0; x < qr.size; x++) if (qrcode_getModule(&qr, x, y)) frame.fillRect(ox + x * sc, oy + y * sc, sc, sc, TFT_BLACK);
  frame.setFont(&fonts::Font0); frame.setTextDatum(textdatum_t::top_center); frame.setTextColor(T.text);
  frame.drawString(url, 120, oy + sz + 12);
  frame.setTextDatum(textdatum_t::bottom_center); frame.drawString(netState() == NetState::PortalAP ? "JOIN WIFI: FLUX-SETUP" : "SETTINGS", 120, oy - 12);
}

static void drawToast() {
  if (millis() > toastUntil) return;
  const Theme& T = themeGet(S.theme);
  frame.setFont(&fonts::Font2); frame.setTextDatum(textdatum_t::middle_center);
  int w = frame.textWidth(toast) + 20;
  frame.fillRoundRect(120 - w / 2, 150, w, 24, 6, cScale(T.accent, 25)); frame.drawRoundRect(120 - w / 2, 150, w, 24, 6, T.accent);
  frame.setTextColor(T.text); frame.drawString(toast, 120, 162);
}

// radial iris transition: new mode frame revealed inside a growing circle
static void drawTransition(uint32_t now) {
  float p = (now - transStart) / 350.0f;
  if (p >= 1) { modes[cur]->exit(); cur = nextMode; nextMode = 255; modes[cur]->enter(); S.mode = cur; settingsSave(); modes[cur]->draw(); return; }
  modes[cur]->draw();
  LGFX_Sprite tmp(&tft); tmp.setPsram(true); tmp.setColorDepth(16);
  if (!tmp.createSprite(240, 240)) return;
  // draw next mode into tmp by temporarily swapping buffers: simplest is draw into frame, copy, redraw
  uint16_t* a = (uint16_t*)frame.getBuffer(); uint16_t* b = (uint16_t*)tmp.getBuffer();
  memcpy(b, a, 240 * 240 * 2);               // b = current
  modes[nextMode]->update(now, 0); modes[nextMode]->draw();   // frame = next
  float r = p * 175; float r2 = r * r;
  for (int y = 0; y < 240; y++) for (int x = 0; x < 240; x++) { float dx = x - 120, dy = y - 120; if (dx * dx + dy * dy > r2) a[y * 240 + x] = b[y * 240 + x]; }
  tmp.deleteSprite();
}

void modesLoop() {
  uint32_t now = millis(); float dt = (now - lastTick) / 1000.0f; lastTick = now; if (dt > 0.5f) dt = 0.5f;
  Btn b = inputPoll();
  if (b == Btn::Long) { qrOn = !qrOn; }
  else if (b != Btn::None && qrOn) { qrOn = false; }
  else if (b == Btn::Triple) { // next enabled mode
    uint8_t n = cur; for (int i = 0; i < MODE_COUNT; i++) { n = (n + 1) % MODE_COUNT; if (modes[n] && ((S.modesEnabled >> n) & 1)) break; }
    if (n != cur) { modeSwitch(n); overlayToast(modeName(n)); }
  }
  else if (b != Btn::None) { modes[cur]->onButton(b); }
  geoLoop(); adsbLoop();
  modes[cur]->update(now, dt);
  bool booting = (netState() == NetState::Connecting || netState() == NetState::Idle) && now < 40000;
  if (booting || netState() == NetState::PortalAP) drawConnectScreen();
  else if (nextMode != 255) drawTransition(now);
  else if (!scheduleActive() && cur == MODE_SKY) {
    if (S.idle == IdleMode::Off) frame.fillSprite(TFT_BLACK); else drawIdleClock();
  }
  else { PulseCard* tk = pulseTakeover(); if (tk) drawPulseCard(*tk, true); else modes[cur]->draw(); }
  if (!booting && netState() != NetState::PortalAP) drawPulseOverlay();
  if (qrOn) drawQR();
  drawToast();
  displayPresent();
}
