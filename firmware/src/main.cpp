#include <Arduino.h>
#include "pins.h"
#include "display.h"
#include "settings.h"
#include "net.h"
#include "web.h"
#include "input.h"
#include "clock.h"
#include "adsb.h"
#include "modes.h"
#include "serialcmd.h"
#include <esp_ota_ops.h>
#include <esp_task_wdt.h>
#include "pulse.h"
#include "auxdata.h"

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== FLUX " FLUX_VERSION " (ESP32-S3 GC9A01) ===");
  { esp_task_wdt_config_t w = { .timeout_ms = 20000, .idle_core_mask = 0, .trigger_panic = true }; esp_task_wdt_reconfigure(&w); esp_task_wdt_add(NULL); }  // watch the UI loop, not IDLE0 (network tasks get priority-boosted under WiFi+BLE coexistence)
  settingsBegin();
  inputBegin();
  displayInit();
  bool holdSetup = inputHeldRaw();
  if (holdSetup) { Serial.println("[boot] BOOT held: forcing setup portal"); netStartPortal(); } else netBegin();
  webBegin();
  adsbBegin();
  auxBegin();
  pulseBegin();
  modesBegin();
  printSettingsSummary();
  Serial.println("type h for help");
}

void loop() {
  static uint32_t lastFrame = 0; static bool ntp = false;
  esp_task_wdt_reset();
  netLoop(); webLoop(); serialCmdLoop();
  if (netOnline() && !ntp) { clockBegin(); ntp = true; }
  uint32_t now = millis();
  if (now - lastFrame >= 33) { lastFrame = now; pulseTick(); modesLoop(); }
  // OTA safety: once we have run 20 s and reached WiFi, declare this image good (cancels bootloader rollback)
  static bool marked = false;
  if (!marked && now > 20000 && netOnline()) { marked = true; esp_ota_mark_app_valid_cancel_rollback(); Serial.println("[ota] image marked valid"); }
  else delay(1);
}
