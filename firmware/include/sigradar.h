#pragma once
#include "modes.h"
struct SignalMode : Mode { const char* name() override { return "SIGNAL"; } void enter() override; void exit() override; void update(uint32_t, float dt) override; void draw() override; bool onButton(Btn b) override; };
extern SignalMode signalMode;
