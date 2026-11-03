#include "pulse.h"
#include "theme.h"
#include <ArduinoJson.h>
#include <esp_heap_caps.h>
static PulseCard cards[PULSE_MAX];
void pulseBegin() { memset(cards, 0, sizeof cards); }
PulseCard* pulseCards() { return cards; }
static uint16_t parseColor(const char* s, uint16_t def) {
  if (!s || s[0] != '#' || strlen(s) != 7) return def;
  long v = strtol(s + 1, nullptr, 16); return c565((v >> 16) & 255, (v >> 8) & 255, v & 255);
}
static PulseCard* find(const char* id) { for (auto& c : cards) if (c.used && !strcmp(c.id, id)) return &c; return nullptr; }
bool pulseUpsert(const String& json, String& err) {
  JsonDocument d; auto e = deserializeJson(d, json); if (e) { err = e.c_str(); return false; }
  const char* id = d["id"] | ""; if (!*id) { err = "id required"; return false; }
  PulseCard* c = find(id);
  if (!c) { for (auto& k : cards) if (!k.used) { c = &k; break; } }
  if (!c) { uint32_t oldest = UINT32_MAX; for (auto& k : cards) if (k.created < oldest) { oldest = k.created; c = &k; } if (c->image) heap_caps_free(c->image); memset(c, 0, sizeof *c); }
  bool fresh = !c->used; if (fresh) { memset(c, 0, sizeof *c); c->created = millis(); }
  c->used = true; strlcpy(c->id, id, sizeof c->id);
  if (d["icon"].is<const char*>() || fresh) strlcpy(c->icon, d["icon"] | "dot", sizeof c->icon);
  if (d["title"].is<const char*>() || fresh) strlcpy(c->title, d["title"] | id, sizeof c->title);
  if (d["text"].is<const char*>() || fresh) strlcpy(c->text, d["text"] | "", sizeof c->text);
  if (d["style"].is<const char*>() || fresh) strlcpy(c->style, d["style"] | "static", sizeof c->style);
  if (d["color"].is<const char*>() || fresh) c->color = parseColor(d["color"] | "", c565(45, 212, 191));
  if (d["progress"].is<int>()) c->progress = constrain(d["progress"].as<int>(), -1, 100); else if (fresh) c->progress = -1;
  if (d["overlay"].is<bool>() || fresh) c->overlay = d["overlay"] | false;
  if (d["takeover"].is<int>()) c->takeover = d["takeover"].as<int>();
  if (d["priority"].is<int>() || fresh) c->priority = d["priority"] | 1;
  uint32_t ttl = d["ttl"] | 300; c->expires = millis() + ttl * 1000UL;
  if (d["takeover"].is<int>()) c->created = millis();   // restart takeover window
  return true;
}
bool pulseSetImage(const char* id, const uint8_t* rgb, size_t len) {
  PulseCard* c = find(id); if (!c || len != 64 * 64 * 2) return false;
  if (!c->image) c->image = (uint16_t*)heap_caps_malloc(64 * 64 * 2, MALLOC_CAP_SPIRAM);
  if (!c->image) return false; memcpy(c->image, rgb, len); return true;
}
bool pulseRemove(const char* id) { PulseCard* c = find(id); if (!c) return false; if (c->image) heap_caps_free(c->image); memset(c, 0, sizeof *c); return true; }
void pulseTick() { uint32_t now = millis(); for (auto& c : cards) if (c.used && (int32_t)(now - c.expires) > 0) { if (c.image) heap_caps_free(c.image); memset(&c, 0, sizeof c); } }
PulseCard* pulseTakeover() {
  uint32_t now = millis(); PulseCard* best = nullptr;
  for (auto& c : cards) if (c.used && c.takeover && now - c.created < c.takeover * 1000UL && (!best || c.priority > best->priority)) best = &c;
  return best;
}
String pulseListJson() {
  JsonDocument d; JsonArray a = d.to<JsonArray>(); uint32_t now = millis();
  for (auto& c : cards) if (c.used) { JsonObject o = a.add<JsonObject>(); o["id"] = c.id; o["icon"] = c.icon; o["title"] = c.title; o["text"] = c.text; o["progress"] = c.progress; o["overlay"] = c.overlay; o["ttl"] = (int)((c.expires - now) / 1000); o["image"] = c.image != nullptr; }
  String s; serializeJson(d, s); return s;
}
