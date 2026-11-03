#pragma once
#include <Arduino.h>
enum class NetState : uint8_t { Idle, Connecting, Connected, PortalAP, Failed };
void netBegin();            // starts STA if configured, else AP portal
void netLoop();
NetState netState();
String netIP();
int netRSSI();
void netStartPortal();      // force AP mode (setup)
bool netOnline();
