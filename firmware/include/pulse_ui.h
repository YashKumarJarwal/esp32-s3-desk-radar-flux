#pragma once
#include "modes.h"
struct PulseMode : Mode {            // full-screen card browser
  const char* name() override { return "PULSE"; }
  void update(uint32_t now, float dt) override;
  void draw() override;
  bool onButton(Btn b) override;
};
extern PulseMode pulseMode;
void drawPulseOverlay();             // small badge(s) on top of any mode
void drawPulseCard(struct PulseCard& c, bool full);
