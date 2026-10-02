#ifndef RED_H
#define RED_H

#include <Arduino.h>

// WiFi (portal cautivo WiFiManager), mDNS y NTP. El riego nunca depende de la
// red: si no hay WiFi u hora de NTP, el programador sigue con el RTC. La hora
// de NTP solo sirve para corregir el RTC de vez en cuando.
void redIniciar();   // en setup(), tras el RTC. Puede abrir el portal (bloqueante con timeout).
void redTick();      // mantiene mDNS y resincroniza el RTC con NTP.

bool redConectada();
int  redRssi();
String redIp();
String redSsid();
bool ntpSincronizado();  // true si el RTC se ha llegado a corregir con NTP

#endif
