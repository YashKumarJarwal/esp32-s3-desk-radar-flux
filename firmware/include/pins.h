#pragma once
// Pin map recovered from the stock firmware boot log (TFT_eSPI setup dump).
#define PIN_TFT_MOSI 11
#define PIN_TFT_SCLK 12
#define PIN_TFT_MISO 13
#define PIN_TFT_DC    7
#define PIN_TFT_RST   6
#define PIN_TFT_CS   -1   // tied low on the board
#define PIN_TFT_BL   -1   // backlight hardwired (no PWM pin known)
#define PIN_BUTTON    0   // BOOT button, active LOW
#define TFT_W 240
#define TFT_H 240
#define TFT_SPI_HZ 40000000
