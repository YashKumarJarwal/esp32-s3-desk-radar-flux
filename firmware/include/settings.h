#pragma once
#include <Arduino.h>

enum class Provider : uint8_t { AdsbFi = 0, OpenSky = 1 };
enum class IdleMode : uint8_t { Off = 0, Clock = 1, Dim = 2 };

struct Settings {
  // --- identity / wifi
  bool configured = false;
  String ssid, wpass;
  String hostname = "flux";
  // --- radar centre & ranges (stock: clat clon range rng0..2 fetchKm edgeDots)
  double lat = 28.64841, lon = 77.10935;
  float rangeKm = 60;                 // active display range
  float preset[3] = {30, 60, 120};    // presets cycled by double tap
  float fetchKm = 75;                 // API query range (clamped display..1.5x, <=300)
  bool edgeDots = true;
  float edgeExtKm = 15;
  // --- display (stock: ui maxac labels bright)
  uint8_t theme = 0;                  // 0 PlaneRadar 1 Classic 2 Cyberpunk 3 Neon(new)
  uint8_t maxAircraft = 12;
  bool labels = true;
  uint8_t brightness = 255;
  // --- data source (stock: prov acct rpol budget reserve cost manint minint os*)
  Provider provider = Provider::AdsbFi;
  uint8_t account = 0;                // 0 anonymous, 1 opensky account
  uint8_t refreshPolicy = 1;          // 0 AutoByDailyBudget 1 ManualInterval
  uint32_t dailyBudget = 400;
  float reserve = 0.90f, cost = 1.0f;
  uint32_t manualIntervalMs = 10000, minIntervalMs = 1000;
  String osUser, osPass, osClientId, osClientSecret;
  // --- schedule (stock: sch start end tz idle)
  bool scheduleEnabled = false;
  int16_t startMin = 480, endMin = 1200, tzMin = 330;
  IdleMode idle = IdleMode::Clock;
  // --- filters (stock: ground minalt minspd)
  bool showGround = false;
  float minAltM = 100, minSpeedMs = 30;
  // --- prediction (stock: pred alpha pmax jump lowspd stale corr cmin cdur cdist)
  bool prediction = true;
  float alpha = 0.15f; uint32_t predMaxMs = 120000; float jumpKm = 20; float lowSpeed = 20; uint32_t staleMs = 120000;
  bool correction = true; uint32_t corrMinApiMs = 30000, corrDurMs = 6000; float corrStartKm = 0.5f;
  // --- hardware (stock: btn dbg txpwr)
  int8_t buttonPin = -1; bool serialDebug = false; uint8_t txPowerRaw = 76;
  // --- ORBIT additions
  uint8_t lang = 0;                   // 0 en 1 zh
  uint8_t mode = 0;                   // active display mode
  uint16_t modesEnabled = 0xFFFF;     // bitmask
  bool trails = true;
  bool lockOn = true;
  float overheadAlertKm = 3.0f;
  bool autoDimNight = true; uint8_t nightBrightness = 40;
  String pulseToken;
};

extern Settings S;
void settingsBegin();                // load; first boot imports stock radar_cfg namespace
bool settingsSave();
void settingsReset();
String settingsToJson(bool includeSecrets);
bool settingsFromJson(const String& json, String& err);
