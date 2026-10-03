#ifndef TAPA_H
#define TAPA_H

#include <Arduino.h>

// LCD 16x2 y los 5 pulsadores de la tapa.
//
// Pantalla de inicio:
//   línea 1: hora y la zona seleccionada, p. ej. "07:12 Huerto 18m"
//   línea 2: las 4 zonas, p. ej. ">1*  2.  3c  4- "
//     '*' regando   'c' abierta para acompañar a otra   'M' interruptor manual
//     '~' maniobra pendiente   '!' relé activado sin tensión en la válvula
//     '.' cerrada   '-' cerrada y sin riego automático
// Izquierda/derecha eligen zona; centro corto abre (DURACION_MANUAL_MIN) o
// detiene la zona elegida; centro largo abre el menú (menu.h).
void tapaIniciar();
void tapaTick();
void tapaMensaje(const char *texto, uint32_t ms = 2000);  // en la línea 1

// Compartidas con el menú
void textoLcd(const char *utf8, char *out, size_t tam);   // quita tildes, ñ de la ROM
void estadoCortoZona(uint8_t n, char *buf, size_t tam);   // "15m", "off", "acp", "man", "err"

#endif
