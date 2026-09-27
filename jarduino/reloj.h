#ifndef RELOJ_H
#define RELOJ_H

#include "rtc.h"

// Hora local leída del DS3231 dos veces por segundo, compartida por el
// programador y la tapa para no leer el RTC desde varios sitios.
void relojTick();
// false si el RTC no responde o perdió la hora: sin hora fiable no se riega
// en automático (el riego manual sigue funcionando).
bool relojValido();
bool relojResponde();
const Fecha &relojAhora();

#endif
