#pragma once
#include <Arduino.h>
#include "input.h"

enum ModeId : uint8_t { MODE_SKY = 0, MODE_CHRONO, MODE_ATMOS, MODE_ORBIT, MODE_SIGNAL, MODE_PULSE, MODE_COUNT };
const char* modeName(uint8_t id);

struct Mode {
  virtual const char* name() = 0;
  virtual void enter() {}
  virtual void exit() {}
  virtual void update(uint32_t now, float dt) {}
  virtual void draw() = 0;
  virtual bool onButton(Btn b) { return false; }   // true = consumed
};

void modesBegin();
void modesLoop();                    // input + update + draw + present
void modeSwitch(uint8_t id);         // with transition
uint8_t modeCurrent();
void overlayQR(bool on); bool overlayQRActive();
void overlayToast(const String& s, uint32_t ms = 2000);
void drawHudStatus();                // shared status line helpers
void drawConnectScreen();            // boot / linking screen
