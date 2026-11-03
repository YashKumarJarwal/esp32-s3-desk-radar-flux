#include "astro.h"
#include <time.h>
static int dayOfYear(int y, int m, int d) { static const int c[] = {0,31,59,90,120,151,181,212,243,273,304,334}; int n = c[m-1] + d; if (m > 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) n++; return n; }
bool sunTimes(int y, int m, int d, double lat, double lon, float& rise, float& set) {
  int doy = dayOfYear(y, m, d);
  double g = 2 * PI / 365.0 * (doy - 1 + 0.5);
  double eqt = 229.18 * (0.000075 + 0.001868 * cos(g) - 0.032077 * sin(g) - 0.014615 * cos(2 * g) - 0.040849 * sin(2 * g));
  double decl = 0.006918 - 0.399912 * cos(g) + 0.070257 * sin(g) - 0.006758 * cos(2 * g) + 0.000907 * sin(2 * g) - 0.002697 * cos(3 * g) + 0.00148 * sin(3 * g);
  double la = lat * DEG_TO_RAD;
  double cosHa = (cos(90.833 * DEG_TO_RAD) / (cos(la) * cos(decl))) - tan(la) * tan(decl);
  if (cosHa > 1 || cosHa < -1) return false;
  double ha = acos(cosHa) * RAD_TO_DEG;
  rise = 720 - 4 * (lon + ha) - eqt; set = 720 - 4 * (lon - ha) - eqt;
  return true;
}
float moonPhase(time_t t) { double days = (t - 947182440.0) / 86400.0; double p = fmod(days / 29.530588853, 1.0); if (p < 0) p += 1; return (float)p; }
