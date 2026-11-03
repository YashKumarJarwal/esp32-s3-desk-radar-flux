#include "serialcmd.h"
#include "settings.h"
#include "net.h"
#include "clock.h"
#include "adsb.h"
#include "modes.h"
#include "sky.h"
#include "input.h"

void printSettingsSummary() {
  Serial.printf("[User] configured=%d ssid=%s centre=%.5f,%.5f range=%.0f presets=%.0f/%.0f/%.0f fetch=%.0f edgeDots=%d\n", S.configured, S.ssid.c_str(), S.lat, S.lon, S.rangeKm, S.preset[0], S.preset[1], S.preset[2], S.fetchKm, S.edgeDots);
  Serial.printf("  provider=%s account=%d policy=%d budget=%u reserve=%.2f cost=%.1f manual=%ums min=%ums computed=%ums\n", S.provider == Provider::AdsbFi ? "adsb.fi" : "OpenSky", S.account, S.refreshPolicy, S.dailyBudget, S.reserve, S.cost, S.manualIntervalMs, S.minIntervalMs, adsbComputedInterval());
  Serial.printf("  schedule=%d start=%d end=%d tz=%d idle=%d theme=%u brightness=%u labels=%d maxAircraft=%u\n", S.scheduleEnabled, S.startMin, S.endMin, S.tzMin, (int)S.idle, S.theme, S.brightness, S.labels, S.maxAircraft);
  Serial.printf("  ground=%d minAlt=%.0f minSpeed=%.1f prediction=%d alpha=%.2f max=%ums jump=%.1fkm lowSpeed=%.1f stale=%ums correction=%d minApi=%ums dur=%ums start=%.1fkm\n", S.showGround, S.minAltM, S.minSpeedMs, S.prediction, S.alpha, S.predMaxMs, S.jumpKm, S.lowSpeed, S.staleMs, S.correction, S.corrMinApiMs, S.corrDurMs, S.corrStartKm);
  Serial.printf("  buttonPin=%d serialDebug=%d txPowerRaw=%u mode=%s trails=%d lockOn=%d\n", S.buttonPin, S.serialDebug, S.txPowerRaw, modeName(S.mode), S.trails, S.lockOn);
}

static void help() {
  Serial.println("Debug serial commands:\n  h: help\n  p: print settings\n  c: enter setup portal\n  x: exit setup portal (reboot)\n  q: toggle settings QR\n  w: show settings URL\n  u: switch UI theme\n  r: switch radar range\n  g: toggle ground traffic\n  o: cycle idle display\n  s: save settings\n  l: load settings\n  t: time/schedule status\n  m: device mode/state\n  a: API/auth status\n  A: clear OpenSky token\n  d: reset settings to default\n  b: reboot\n  n: next display mode\n  f: fetch now\n  1/2/3: virtual short/double/long press");
}

void serialCmdLoop() {
  while (Serial.available()) {
    char c = Serial.read();
    switch (c) {
      case 'h': help(); break;
      case 'p': printSettingsSummary(); break;
      case 'c': netStartPortal(); break;
      case 'x': ESP.restart(); break;
      case 'q': overlayQR(!overlayQRActive()); break;
      case 'w': Serial.printf("http://%s/\n", netIP().c_str()); break;
      case 'u': skyMode.nextTheme(); break;
      case 'r': skyMode.nextRange(); break;
      case 'g': S.showGround = !S.showGround; settingsSave(); Serial.printf("ground=%d\n", S.showGround); break;
      case 'o': S.idle = (IdleMode)(((int)S.idle + 1) % 3); settingsSave(); Serial.printf("idle=%d\n", (int)S.idle); break;
      case 's': Serial.println(settingsSave() ? "saved" : "save failed"); break;
      case 'l': settingsBegin(); break;
      case 't': { int h, m, s; clockLocal(h, m, s); Serial.printf("synced=%d local=%02d:%02d:%02d tz=%d schedule=%d active=%d\n", clockSynced(), h, m, s, S.tzMin, S.scheduleEnabled, scheduleActive()); break; }
      case 'm': Serial.printf("mode=%s net=%d ip=%s rssi=%d heap=%u psram=%u\n", modeName(modeCurrent()), (int)netState(), netIP().c_str(), netRSSI(), ESP.getFreeHeap(), ESP.getFreePsram()); break;
      case 'a': { const AdsbStats& s = adsbStats(); Serial.printf("provider=%s status=%d http=%d dur=%ums payload=%u raw=%u valid=%u active=%u interval=%ums today=%u err=%s\n", s.provider, (int)s.status, s.httpCode, s.durMs, s.payload, s.raw, s.valid, s.active, s.intervalMs, s.requestsToday, s.err); break; }
      case 'A': adsbClearOpenSkyToken(); Serial.println("token cleared"); break;
      case 'd': settingsReset(); settingsSave(); Serial.println("settings reset"); break;
      case 'b': ESP.restart(); break;
      case 'n': modeSwitch((modeCurrent() + 1) % MODE_COUNT); break;
      case 'f': adsbRequestNow(); break;
      case '1': inputInject(Btn::Short); break;
      case '2': inputInject(Btn::Double); break;
      case '3': inputInject(Btn::Long); break;
      case '4': inputInject(Btn::Triple); break;
      default: break;
    }
  }
}
