#pragma once
#include <Arduino.h>
enum class Btn : uint8_t { None, Short, Double, Triple, Long, LongRelease };
void inputBegin();
void inputService();             // call as often as possible (main loop); runs the debounce state machine
Btn inputPoll();                 // returns the next queued event (called once per frame)
void inputInject(Btn b);         // from serial / web (virtual buttons)
bool inputHeldRaw();
