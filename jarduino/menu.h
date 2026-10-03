#ifndef MENU_H
#define MENU_H

#include <Arduino.h>

// Menú de la tapa (centro largo desde la pantalla de inicio):
//   Zonas > Zona n > Inicio, Fin, Dias, Regar ahora, Parar
//   Parar todo
//   Fecha y hora
//   Sistema (IP, WiFi, RSSI, NTP, tiempo encendido, firmware)
// Arriba/abajo cambian de opción o de valor; derecha entra o pasa al campo
// siguiente; izquierda vuelve atrás (al editar, campo anterior o cancelar);
// centro corto confirma; centro largo vuelve a la pantalla de inicio sin
// guardar. Tras MENU_INACTIVIDAD_MS sin tocar nada también vuelve al inicio.
// Lo que se edita mejor desde la web (nombres, activo, compañía,
// notificaciones) no está en el menú.

enum class Tecla : uint8_t { Arriba, Abajo, Izq, Der, Centro, CentroLargo };

void menuAbrir();
bool menuActivo();
void menuTecla(Tecla t);
void menuTick();
// Las dos líneas de la pantalla del menú y la columna del cursor parpadeante
// en la línea 2 (-1 si no se está editando).
void menuPantalla(char *linea0, char *linea1, size_t tam, int8_t &cursor);

#endif
