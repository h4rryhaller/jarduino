#ifndef CONFIGURACION_H
#define CONFIGURACION_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "config.h"

// Espejo en memoria de /config.json (formato en la sección 11 del documento
// de diseño). Solo configuración: el estado del riego vive en riego.cpp.

struct Horario {
  uint16_t inicio;  // minutos desde medianoche, hora local
  uint16_t fin;     // fin <= inicio: esa zona no riega en automático
  uint8_t dias;     // bit 0 = lunes ... bit 6 = domingo
};

struct ZonaConfig {
  uint8_t id;  // 1..NUM_ZONAS
  char nombre[24];
  bool activo;
  uint8_t compania;  // bit (n-1) = zona n; 0 si puede regar sola
  Horario horario;
};

struct Notificaciones {
  bool activo;
  char servicio[12];  // "ninguno" | "ntfy" | "telegram"
  char destino[64];
};

struct Configuracion {
  uint8_t version;
  ZonaConfig zonas[NUM_ZONAS];
  Notificaciones notificaciones;
};

extern Configuracion config;

void aplicarDefectos(Configuracion &c);
// Monta LittleFS (lo formatea si no monta) y carga CONFIG_FILE_PATH. Si falta
// o no se puede leer, aplica los valores por defecto y los guarda.
bool cargarConfiguracion(Configuracion &c);
bool guardarConfiguracion(const Configuracion &c);

// Compartidos con la futura API para no duplicar el (de)serializado.
void zonaAJson(const ZonaConfig &z, JsonObject obj);
// Aplica sobre z los campos presentes en obj. Devuelve false (sin tocar z) si
// algún campo presente no es válido.
bool zonaDesdeJson(JsonObjectConst obj, ZonaConfig &z);
void configuracionAJson(const Configuracion &c, JsonDocument &doc);

#endif
