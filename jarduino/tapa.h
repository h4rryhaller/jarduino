#ifndef TAPA_H
#define TAPA_H

// LCD 16x2 y los 5 pulsadores de la tapa.
//
// Pantalla de inicio:
//   línea 1: hora y la zona seleccionada, p. ej. "07:12 Huerto 18m"
//   línea 2: las 4 zonas, p. ej. ">1*  2.  3c  4- "
//     '*' regando   'c' abierta para acompañar a otra   'M' interruptor manual
//     '~' maniobra pendiente   '!' relé activado sin tensión en la válvula
//     '.' cerrada   '-' cerrada y sin riego automático
// Izquierda/derecha eligen zona; centro corto abre (DURACION_MANUAL_MIN) o
// detiene la zona elegida; centro largo, el menú (pendiente).
void tapaIniciar();
void tapaTick();
void tapaMensaje(const char *texto, uint32_t ms = 2000);

#endif
