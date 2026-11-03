#pragma once
#include "modes.h"
struct SkyMode : Mode {
  const char* name() override { return "SKY"; }
  void enter() override;
  void update(uint32_t now, float dt) override;
  void draw() override;
  bool onButton(Btn b) override;
  void nextRange(); void nextTheme();
};
extern SkyMode skyMode;
void drawIdleClock();  // schedule-off screen
