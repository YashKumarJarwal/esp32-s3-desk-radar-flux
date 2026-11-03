#pragma once
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "pins.h"

class FluxGFX : public lgfx::LGFX_Device {
  lgfx::Panel_GC9A01 _panel;
  lgfx::Bus_SPI _bus;
public:
  FluxGFX() {
    auto bc = _bus.config();
    bc.spi_host = SPI2_HOST;
    bc.spi_mode = 0;
    bc.freq_write = TFT_SPI_HZ;
    bc.freq_read = 16000000;
    bc.spi_3wire = false;
    bc.use_lock = true;
    bc.dma_channel = SPI_DMA_CH_AUTO;
    bc.pin_sclk = PIN_TFT_SCLK;
    bc.pin_mosi = PIN_TFT_MOSI;
    bc.pin_miso = PIN_TFT_MISO;
    bc.pin_dc = PIN_TFT_DC;
    _bus.config(bc);
    _panel.setBus(&_bus);
    auto pc = _panel.config();
    pc.pin_cs = PIN_TFT_CS;
    pc.pin_rst = PIN_TFT_RST;
    pc.pin_busy = -1;
    pc.panel_width = TFT_W;
    pc.panel_height = TFT_H;
    pc.offset_x = 0; pc.offset_y = 0; pc.offset_rotation = 0;
    pc.readable = false;
    pc.invert = true;
    pc.rgb_order = false;
    pc.dlen_16bit = false;
    pc.bus_shared = false;
    _panel.config(pc);
    setPanel(&_panel);
  }
};

// Full-frame RGB565 canvas living in PSRAM; everything draws here, then one DMA push per frame.
extern FluxGFX tft;
extern LGFX_Sprite frame;
void displayInit();
void displayPresent();        // push frame -> panel
const uint16_t* frameBuffer(); // for the screen mirror
