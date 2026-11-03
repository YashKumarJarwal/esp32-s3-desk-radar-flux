#pragma once
#include "modes.h"
struct FlightMode : Mode {
  const char* name() override { return "FLIGHT"; }
  void enter() override; void update(uint32_t, float dt) override; void draw() override; bool onButton(Btn b) override;
};
extern FlightMode flightMode;
