#ifndef CONSOLA_H
#define CONSOLA_H

// Órdenes por el monitor serie (115200, fin de línea "Nueva línea"), para
// probar en el banco sin red:
//   T2026-09-24 18:30:00   pone en hora el RTC
//   R 3 10                 riega la zona 3 durante 10 minutos
//   P 3  /  P              detiene la zona 3 / todas
//   E                      estado de las zonas
//   C                      config.json actual
//   W                      estado de la WiFi (conectado, SSID, IP, RSSI, NTP)
void consolaTick();

#endif
