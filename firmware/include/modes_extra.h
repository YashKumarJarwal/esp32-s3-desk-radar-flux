#pragma once
#include "modes.h"
struct ChronoMode : Mode { const char* name() override { return "CHRONO"; } void update(uint32_t, float dt) override; void draw() override; };
struct AtmosMode  : Mode { const char* name() override { return "ATMOS"; }  void update(uint32_t, float dt) override; void draw() override; bool onButton(Btn b) override; };
struct OrbitMode  : Mode { const char* name() override { return "ORBIT"; }  void update(uint32_t, float dt) override; void draw() override; };
extern ChronoMode chronoMode; extern AtmosMode atmosMode; extern OrbitMode orbitMode;
