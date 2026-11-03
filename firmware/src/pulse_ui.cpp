#include "pulse_ui.h"
#include "pulse.h"
#include "display.h"
#include "settings.h"
#include "theme.h"
#include "clock.h"

PulseMode pulseMode;
static float anim = 0; static int sel = 0;

// ---- vector icons (drawn, no bitmaps): ai, github, music, pr, bell, check, warn, ci, dot
static void icon(const char* name, int x, int y, int r, uint16_t col, float t, const char* style) {
  const Theme& T = themeGet(S.theme);
  float pulse = 0.5f + 0.5f * sinf(t * 4);
  bool pulsing = !strcmp(style, "pulse") || !strcmp(style, "think");
  if (!strcmp(name, "ai") || !strcmp(name, "claude")) {
    // thinking orb: nucleus + 3 orbiting electrons + breathing halo
    int rr = r * (pulsing ? 0.55f + 0.1f * pulse : 0.6f);
    for (int i = 3; i >= 1; i--) frame.drawCircle(x, y, rr + i * 3, cLerp(T.bg, col, 60 / i));
    frame.fillCircle(x, y, rr, col); frame.fillCircle(x - rr / 3, y - rr / 3, rr / 3, cLerp(col, TFT_WHITE, 120));
    for (int k = 0; k < 3; k++) { float a = t * (1.5f + k * 0.4f) + k * 2.1f; float ex = x + cosf(a) * r, ey = y + sinf(a) * r * 0.45f; frame.drawEllipse(x, y, r, r * 0.45f, cLerp(T.bg, col, 70)); frame.fillCircle(ex, ey, 2, cLerp(col, TFT_WHITE, 80)); }
  } else if (!strcmp(name, "github") || !strcmp(name, "pr")) {
    // branch glyph: two nodes + merge curve
    frame.drawWideLine(x - r * 0.5f, y - r * 0.7f, x - r * 0.5f, y + r * 0.7f, 2, col);
    frame.drawBezier(x + r * 0.5f, y - r * 0.6f, x + r * 0.5f, y + r * 0.2f, x - r * 0.5f, y + r * 0.3f, col);
    frame.fillCircle(x - r * 0.5f, y - r * 0.7f, 4, col); frame.fillCircle(x - r * 0.5f, y + r * 0.7f, 4, col); frame.fillCircle(x + r * 0.5f, y - r * 0.6f, 4, col);
  } else if (!strcmp(name, "music") || !strcmp(name, "spotify")) {
    // equaliser bars
    for (int i = 0; i < 5; i++) { float h = r * (0.35f + 0.65f * fabsf(sinf(t * 3 + i * 1.3f))); if (strcmp(style, "pulse") && strcmp(style, "play")) h = r * (0.3f + 0.15f * i); frame.fillRoundRect(x - r + i * (r * 2 / 5) + 1, y + r - h * 2 + r * 0.0f - r, r * 2 / 5 - 3, h * 2, 2, col); }
  } else if (!strcmp(name, "bell")) {
    frame.fillArc(x, y + r * 0.2f, 0, r * 0.8f, 180, 360, col); frame.fillRect(x - r * 0.8f, y + r * 0.2f, r * 1.6f, 3, col); frame.fillCircle(x, y + r * 0.55f, r * 0.2f, col);
  } else if (!strcmp(name, "check")) {
    frame.drawWideLine(x - r * 0.6f, y, x - r * 0.15f, y + r * 0.5f, 3, col); frame.drawWideLine(x - r * 0.15f, y + r * 0.5f, x + r * 0.7f, y - r * 0.5f, 3, col);
  } else if (!strcmp(name, "warn")) {
    frame.drawTriangle(x, y - r, x - r, y + r * 0.8f, x + r, y + r * 0.8f, col); frame.fillRect(x - 1, y - r * 0.4f, 3, r * 0.7f, col); frame.fillCircle(x, y + r * 0.5f, 2, col);
  } else if (!strcmp(name, "ci") || !strcmp(name, "build")) {
    float a0 = t * 180; frame.drawArc(x, y, r * 0.8f, r, a0, a0 + 250, col); frame.drawArc(x, y, r * 0.5f, r * 0.65f, -a0, -a0 + 200, cLerp(T.bg, col, 150));
  } else { // dot
    frame.fillCircle(x, y, r * (pulsing ? 0.5f + 0.3f * pulse : 0.6f), col);
  }
}

static void progressRing(int x, int y, int r, int pct, uint16_t col, uint16_t dim) {
  frame.drawArc(x, y, r - 3, r, 0, 360, dim);
  if (pct > 0) frame.fillArc(x, y, r - 3, r, -90, -90 + 360 * pct / 100, col);
}

void drawPulseCard(PulseCard& c, bool full) {
  const Theme& T = themeGet(S.theme);
  if (full) {
    frame.fillSprite(T.bg);
    // ambient halo in card colour
    for (int i = 6; i >= 1; i--) frame.drawCircle(120, 120, 100 + i * 3, cLerp(T.bg, c.color, 18 * (7 - i)));
    if (c.image) { frame.pushImage(88, 44, 64, 64, c.image); frame.drawRoundRect(87, 43, 66, 66, 6, cLerp(T.bg, c.color, 120)); }
    else icon(c.icon, 120, 78, 26, c.color, anim, c.style);
    if (c.progress >= 0) progressRing(120, 120, 112, c.progress, c.color, T.ringDim);
    frame.setTextDatum(textdatum_t::middle_center);
    frame.setFont(&fonts::Orbitron_Light_24); frame.setTextColor(T.text);
    String title = c.title; if (frame.textWidth(title) > 200) { frame.setFont(&fonts::Font4); }
    frame.drawString(title, 120, 132);
    frame.setFont(&fonts::Font2); frame.setTextColor(cLerp(T.text, c.color, 90));
    String txt = c.text;
    if (frame.textWidth(txt) > 210) { // simple two-line wrap
      int cut = txt.lastIndexOf(' ', txt.length() / 2 + 6); if (cut < 1) cut = txt.length() / 2;
      frame.drawString(txt.substring(0, cut), 120, 160); frame.drawString(txt.substring(cut + 1), 120, 178);
    } else frame.drawString(txt, 120, 162);
    frame.setFont(&fonts::Font0); frame.setTextColor(T.labelDim);
    frame.drawString(clockSynced() ? clockHHMM() : "", 120, 208);
  } else {
    // compact badge, tucked into the top-left rim of the round screen
    int x = 44, y = 44;
    frame.fillCircle(x, y, 12, cLerp(T.bg, c.color, 40)); frame.drawCircle(x, y, 12, cLerp(T.bg, c.color, 140));
    icon(c.icon, x, y, 6, c.color, anim, c.style);
    if (c.progress >= 0) progressRing(x, y, 14, c.progress, c.color, cLerp(T.bg, c.color, 60));
  }
}

void drawPulseOverlay() {
  anim += 0.033f;
  PulseCard* cs = pulseCards(); int n = 0;
  for (int i = 0; i < PULSE_MAX; i++) if (cs[i].used && cs[i].overlay) {
    // stack badges along the top-left rim
    PulseCard& c = cs[i]; int x = 44 + n * 26, y = 44 - n * 12;
    if (n == 0) drawPulseCard(c, false);
    else { frame.fillCircle(x, y, 8, cLerp(themeGet(S.theme).bg, c.color, 40)); icon(c.icon, x, y, 4, c.color, anim, c.style); }
    if (++n >= 3) break;
  }
}

void PulseMode::update(uint32_t now, float dt) { anim += dt; }
bool PulseMode::onButton(Btn b) {
  if (b == Btn::Short) { sel++; return true; }
  return false;
}
void PulseMode::draw() {
  const Theme& T = themeGet(S.theme);
  PulseCard* cs = pulseCards(); int n = 0; PulseCard* list[PULSE_MAX];
  for (int i = 0; i < PULSE_MAX; i++) if (cs[i].used) list[n++] = &cs[i];
  if (!n) {
    frame.fillSprite(T.bg); frame.setTextDatum(textdatum_t::middle_center);
    icon("ai", 120, 90, 24, T.accent, anim, "pulse");
    frame.setFont(&fonts::Font2); frame.setTextColor(T.text); frame.drawString("PULSE", 120, 134);
    frame.setFont(&fonts::Font0); frame.setTextColor(T.labelDim); frame.drawString("no cards - push via /api/pulse", 120, 156); frame.drawString("bridge: ~/deskradar/bridge", 120, 168);
    return;
  }
  drawPulseCard(*list[sel % n], true);
  if (n > 1) for (int i = 0; i < n; i++) frame.fillCircle(120 - (n - 1) * 5 + i * 10, 226, 2, i == sel % n ? T.accent : T.ringDim);
}
