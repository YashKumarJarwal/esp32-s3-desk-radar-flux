#include "input.h"
#include "settings.h"
#include "pins.h"

static int pin = PIN_BUTTON;
static bool last = false; static uint32_t tDown = 0, tUp = 0, tDeb = 0;
static uint8_t clicks = 0; static bool longFired = false;
static Btn injected = Btn::None; static Btn queue[4]; static uint8_t qn = 0;
static void push(Btn b) { if (qn < 4) queue[qn++] = b; }
static const uint32_t DEBOUNCE = 30, DOUBLE_GAP = 500, LONG_MS = 1500;

void inputBegin() { pin = S.buttonPin >= 0 ? S.buttonPin : PIN_BUTTON; pinMode(pin, INPUT_PULLUP); }
void inputInject(Btn b) { injected = b; }
bool inputHeldRaw() { return digitalRead(pin) == LOW; }

void inputService() {
  uint32_t now = millis();
  if (now < 2500) { last = digitalRead(pin) == LOW; clicks = 0; return; }  // ignore flasher-induced presses right after boot
  bool down = digitalRead(pin) == LOW;
  if (down != last && now - tDeb > DEBOUNCE) {
    tDeb = now; last = down;
    if (down) { tDown = now; longFired = false; }
    else { tUp = now; if (longFired) { push(Btn::LongRelease); return; } clicks++; }
  }
  if (down && !longFired && now - tDown > LONG_MS) { longFired = true; clicks = 0; push(Btn::Long); }
  if (!down && clicks && now - tUp > DOUBLE_GAP) { push(clicks >= 3 ? Btn::Triple : (clicks == 2 ? Btn::Double : Btn::Short)); clicks = 0; }
}
Btn inputPoll() {
  if (injected != Btn::None) { Btn b = injected; injected = Btn::None; return b; }
  if (!qn) return Btn::None;
  Btn b = queue[0]; for (int i = 1; i < qn; i++) queue[i - 1] = queue[i]; qn--; return b;
}
