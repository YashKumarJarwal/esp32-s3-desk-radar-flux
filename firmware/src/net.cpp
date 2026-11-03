#include "net.h"
#include "settings.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <DNSServer.h>

static NetState st = NetState::Idle;
static uint32_t tConn = 0;
static DNSServer dns;
static bool dnsOn = false;
static uint8_t retries = 0;

static String apName() { uint64_t m = ESP.getEfuseMac(); char b[24]; snprintf(b, sizeof b, "FLUX-%04X", (unsigned)((m >> 24) & 0xFFFF)); return b; }

void netStartPortal() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(apName().c_str());
  dns.start(53, "*", WiFi.softAPIP()); dnsOn = true;
  st = NetState::PortalAP;
  Serial.printf("[net] portal AP %s at %s\n", apName().c_str(), WiFi.softAPIP().toString().c_str());
}

static void startSTA() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(S.hostname.c_str());
  WiFi.setSleep(true);
  WiFi.setTxPower((wifi_power_t)S.txPowerRaw);
  WiFi.begin(S.ssid.c_str(), S.wpass.c_str());
  tConn = millis(); st = NetState::Connecting;
  Serial.printf("[net] connecting to %s\n", S.ssid.c_str());
}

void netBegin() {
  if (S.configured && S.ssid.length()) startSTA(); else netStartPortal();
}

void netLoop() {
  if (dnsOn) dns.processNextRequest();
  if (st == NetState::Connecting) {
    if (WiFi.status() == WL_CONNECTED) {
      st = NetState::Connected; retries = 0;
      Serial.printf("[net] connected IP=%s RSSI=%d\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
      MDNS.begin(S.hostname.c_str()); MDNS.addService("http", "tcp", 80);
    } else if (millis() - tConn > 25000) {
      if (++retries >= 3) { Serial.println("[net] giving up, starting portal"); netStartPortal(); }
      else { Serial.println("[net] timeout, retry"); WiFi.disconnect(); startSTA(); }
    }
  } else if (st == NetState::Connected && WiFi.status() != WL_CONNECTED) {
    Serial.println("[net] link lost, reconnecting"); startSTA();
  }
}
NetState netState() { return st; }
bool netOnline() { return st == NetState::Connected; }
String netIP() { return st == NetState::PortalAP ? WiFi.softAPIP().toString() : WiFi.localIP().toString(); }
int netRSSI() { return WiFi.RSSI(); }
