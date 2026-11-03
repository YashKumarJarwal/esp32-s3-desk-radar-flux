#include "display.h"
FluxGFX tft;
LGFX_Sprite frame(&tft);
static bool s_ok = false;

void displayInit() {
  tft.init();
  tft.setRotation(0);
  tft.setColorDepth(16);
  tft.fillScreen(TFT_BLACK);
  frame.setPsram(true);
  frame.setColorDepth(16);
  s_ok = frame.createSprite(TFT_W, TFT_H) != nullptr;
  frame.setSwapBytes(false);
  Serial.printf("[display] GC9A01 init, frame buffer %s (PSRAM free %u)\n", s_ok ? "ok" : "FAILED", ESP.getFreePsram());
}
#include "settings.h"
// Backlight is hardwired on this board: brightness is applied in software by scaling the frame before the push.
void displayPresent() {
  if (!s_ok) return;
  uint8_t br = S.brightness;
  if (br < 255) {
    uint16_t* p = (uint16_t*)frame.getBuffer(); uint32_t n = TFT_W * TFT_H; uint32_t k = br + 1;
    for (uint32_t i = 0; i < n; i++) {
      uint16_t c = __builtin_bswap16(p[i]);
      uint32_t r = ((c >> 11) * k) >> 8, g = (((c >> 5) & 63) * k) >> 8, b = ((c & 31) * k) >> 8;
      p[i] = __builtin_bswap16((r << 11) | (g << 5) | b);
    }
  }
  frame.pushSprite(0, 0);
}
const uint16_t* frameBuffer() { return (const uint16_t*)frame.getBuffer(); }
