#pragma once
#include <Arduino.h>
void clockBegin();                 // NTP
bool clockSynced();
void clockLocal(int& h, int& m, int& s);   // local time using S.tzMin
int  clockLocalMinutes();          // minutes since local midnight
bool scheduleActive();             // true if API/display should run now (stock schedule semantics)
String clockHHMM();
time_t clockNow();
