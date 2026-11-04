#pragma once
#include <Arduino.h>
// 3D globe renderer: map points precomputed as unit vectors in PSRAM, one rotation per frame, day/night shading.
struct GlobeCam { double lat, lon; float R; };
void globeInit();
void globeSetSun(time_t utc);                                        // subsolar point from time
void globeDrawCoast(const GlobeCam& c, int cx, int cy, float Rview, uint16_t day, uint16_t night);
void globeDrawBorders(const GlobeCam& c, int cx, int cy, float Rview, uint16_t day, uint16_t night);
bool globeProject(const GlobeCam& c, double lat, double lon, int cx, int cy, float& x, float& y, float* depth = nullptr);
const char* globeCountryAt(double lat, double lon);                 // point-in-polygon over Natural Earth countries, or ""
int globeCitiesNear(double la1, double lo1, double la2, double lo2, int maxN, int* outIdx);   // cities within ~200 km of the great-circle arc, by population
void globeCity(int idx, double& lat, double& lon, char* name, size_t n);
