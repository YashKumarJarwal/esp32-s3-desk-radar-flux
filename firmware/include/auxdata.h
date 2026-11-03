#pragma once
#include <Arduino.h>
struct Weather { bool valid; float tempC, feelsC, humidity, windKmh, pm25, pm10; int code, aqi, isDay; float hourT[24]; int hourP[24]; int hours; uint32_t at; };
struct IssFix { bool valid; double lat, lon; float altKm, velKmh; char visibility[12]; float distKm, bearing, elevDeg; uint32_t at; };
#define ISS_TRAIL 40
struct RouteInfo { char callsign[9]; char airline[24]; char from[5]; char to[5]; char fromCity[20]; char toCity[20]; bool valid; uint32_t at; };
void auxBegin();
const Weather& auxWeather();
const IssFix& auxIss();
const double* auxIssTrail(int& count, int& head);   // lat,lon pairs
const RouteInfo* auxRoute(const char* callsign);     // cached or nullptr (queues lookup)
void httpsLock(); void httpsUnlock();                 // one TLS session at a time
