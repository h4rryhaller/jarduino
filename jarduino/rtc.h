#ifndef RTC_H
#define RTC_H

#include <Arduino.h>

// DS3231 en ADDR_RTC, siempre en hora local y modo 24 h.
struct Fecha {
  uint16_t anio;
  uint8_t mes, dia, hora, min, seg;
};

bool rtcLeer(Fecha &f);
bool rtcEscribir(const Fecha &f);
// true si el oscilador se paró (sin pila o pila agotada): la hora no es fiable
// hasta que se vuelva a poner en hora.
bool rtcPerdioHora();

bool fechaValida(const Fecha &f);
// 0 = lunes ... 6 = domingo (mismo orden que "L M X J V S D")
uint8_t diaSemana(const Fecha &f);

#endif
