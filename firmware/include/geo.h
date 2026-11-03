#pragma once
#include <Arduino.h>
struct NearAirport { int idx; float distKm; double lat, lon; char icao[5]; char iata[4]; };
void geoLoop();                                             // refresh nearby airport cache when centre/range changes
const NearAirport* geoNearby(int& count);                   // airports within ~1.5x display range
const NearAirport* geoNearestTo(double lat, double lon, float maxKm);
void geoDrawRunways(void (*toXY)(double, double, float&, float&), uint16_t col, uint16_t labelCol, bool labels);
void geoDrawCoast(int cx, int cy, float R, double lat0, double lon0, uint16_t col);   // orthographic globe coastlines
bool geoProject(double lat, double lon, double lat0, double lon0, int cx, int cy, float R, float& x, float& y);
void geoGreatCircle(double la1, double lo1, double la2, double lo2, int n, double* outLatLon);  // n+1 points
