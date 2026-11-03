#pragma once
#include <Arduino.h>
#define PULSE_MAX 8
struct PulseCard {
  bool used; char id[16]; char icon[12]; char title[24]; char text[48]; char style[10]; uint16_t color;
  int8_t progress;            // -1 none, 0..100
  bool overlay;               // small badge on any screen
  uint16_t takeover;          // seconds of full-screen, 0 = none
  uint32_t created, expires;  // millis
  uint16_t* image;            // optional 64x64 RGB565 in PSRAM
  uint8_t priority;
};
void pulseBegin();
bool pulseUpsert(const String& json, String& err);
bool pulseSetImage(const char* id, const uint8_t* rgb565, size_t len);
bool pulseRemove(const char* id);
void pulseTick();                      // expire
PulseCard* pulseCards();
PulseCard* pulseTakeover();            // card currently demanding the screen, or null
String pulseListJson();
