#pragma once
#include <Arduino.h>
enum class Btn : uint8_t { None, Short, Double, Triple, Long, LongRelease };
void inputBegin();
Btn inputPoll();                 // call every loop; returns one event at a time
void inputInject(Btn b);         // from serial / web (virtual buttons)
bool inputHeldRaw();
