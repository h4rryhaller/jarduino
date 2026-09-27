#include <LittleFS.h>
#include "configuracion.h"

Configuracion config;

static const char LETRAS_DIAS[] = "LMXJVSD";
static const uint8_t TODOS_LOS_DIAS = 0x7F;

static void zonaPorDefecto(ZonaConfig &z, uint8_t id, const char *nombre, bool activo,
                           uint8_t compania, uint16_t inicio, uint16_t fin, uint8_t dias) {
  z.id = id;
  strlcpy(z.nombre, nombre, sizeof(z.nombre));
  z.activo = activo;
  z.compania = compania;
  z.horario = {inicio, fin, dias};
}

void aplicarDefectos(Configuracion &c) {
  c.version = 1;
  zonaPorDefecto(c.zonas[0], 1, "Huerto", true, 0, 7 * 60, 7 * 60 + 20, TODOS_LOS_DIAS);
  zonaPorDefecto(c.zonas[1], 2, "Huerta", true, 0, 7 * 60 + 20, 7 * 60 + 40, TODOS_LOS_DIAS);
  // Jardín: L, X, V; nunca solo (zona 1 o 2)
  zonaPorDefecto(c.zonas[2], 3, "Jardín", true, 0b011, 7 * 60, 7 * 60 + 40, 0b0010101);
  zonaPorDefecto(c.zonas[3], 4, "Sin asignar", false, 0, 0, 0, 0);
  c.notificaciones.activo = false;
  strlcpy(c.notificaciones.servicio, "ninguno", sizeof(c.notificaciones.servicio));
  c.notificaciones.destino[0] = '\0';
}

static bool horaDesdeTexto(const char *texto, uint16_t &minutos) {
  unsigned h, m;
  char resto;
  if (!texto || sscanf(texto, "%u:%u%c", &h, &m, &resto) != 2) return false;
  if (h > 23 || m > 59) return false;
  minutos = h * 60 + m;
  return true;
}

static void horaATexto(uint16_t minutos, char *buf, size_t tam) {
  snprintf(buf, tam, "%02u:%02u", minutos / 60, minutos % 60);
}

bool zonaDesdeJson(JsonObjectConst obj, ZonaConfig &z) {
  ZonaConfig nueva = z;

  if (obj["nombre"].is<const char *>()) {
    const char *nombre = obj["nombre"];
    if (strlen(nombre) == 0 || strlen(nombre) >= sizeof(nueva.nombre)) return false;
    strlcpy(nueva.nombre, nombre, sizeof(nueva.nombre));
  } else if (!obj["nombre"].isNull()) {
    return false;
  }

  if (obj["activo"].is<bool>()) nueva.activo = obj["activo"];
  else if (!obj["activo"].isNull()) return false;

  if (obj["necesita_compania"].is<JsonArrayConst>()) {
    nueva.compania = 0;
    for (JsonVariantConst v : obj["necesita_compania"].as<JsonArrayConst>()) {
      int id = v | 0;
      if (id < 1 || id > NUM_ZONAS || id == nueva.id) return false;
      nueva.compania |= 1 << (id - 1);
    }
  } else if (!obj["necesita_compania"].isNull()) {
    return false;
  }

  JsonObjectConst horario = obj["horario"];
  if (!horario.isNull()) {
    if (!horario["inicio"].isNull() && !horaDesdeTexto(horario["inicio"], nueva.horario.inicio)) return false;
    if (!horario["fin"].isNull() && !horaDesdeTexto(horario["fin"], nueva.horario.fin)) return false;
    if (horario["dias"].is<JsonArrayConst>()) {
      nueva.horario.dias = 0;
      for (JsonVariantConst v : horario["dias"].as<JsonArrayConst>()) {
        const char *letra = v | "";
        const char *p = strlen(letra) == 1 ? strchr(LETRAS_DIAS, letra[0]) : nullptr;
        if (!p) return false;
        nueva.horario.dias |= 1 << (p - LETRAS_DIAS);
      }
    } else if (!horario["dias"].isNull()) {
      return false;
    }
  } else if (!obj["horario"].isNull()) {
    return false;
  }

  z = nueva;
  return true;
}

void zonaAJson(const ZonaConfig &z, JsonObject obj) {
  obj["id"] = z.id;
  obj["nombre"] = z.nombre;
  obj["activo"] = z.activo;
  JsonArray compania = obj["necesita_compania"].to<JsonArray>();
  for (uint8_t i = 0; i < NUM_ZONAS; i++)
    if (z.compania & (1 << i)) compania.add(i + 1);

  JsonObject horario = obj["horario"].to<JsonObject>();
  char hora[8];
  horaATexto(z.horario.inicio, hora, sizeof(hora));
  horario["inicio"] = hora;
  horaATexto(z.horario.fin, hora, sizeof(hora));
  horario["fin"] = hora;
  JsonArray dias = horario["dias"].to<JsonArray>();
  for (uint8_t d = 0; d < 7; d++)
    if (z.horario.dias & (1 << d)) dias.add(String(LETRAS_DIAS[d]));
}

void configuracionAJson(const Configuracion &c, JsonDocument &doc) {
  doc["version"] = c.version;
  JsonArray zonas = doc["zonas"].to<JsonArray>();
  for (uint8_t i = 0; i < NUM_ZONAS; i++) zonaAJson(c.zonas[i], zonas.add<JsonObject>());

  JsonObject notif = doc["sistema"]["notificaciones"].to<JsonObject>();
  notif["activo"] = c.notificaciones.activo;
  notif["servicio"] = c.notificaciones.servicio;
  notif["destino"] = c.notificaciones.destino;
}

static void configuracionDesdeJson(const JsonDocument &doc, Configuracion &c) {
  c.version = doc["version"] | c.version;

  for (JsonObjectConst obj : doc["zonas"].as<JsonArrayConst>()) {
    int id = obj["id"] | 0;
    if (id < 1 || id > NUM_ZONAS) continue;
    if (!zonaDesdeJson(obj, c.zonas[id - 1]))
      Serial.printf("config.json: zona %d con campos no válidos, se queda por defecto\n", id);
  }

  JsonObjectConst notif = doc["sistema"]["notificaciones"];
  c.notificaciones.activo = notif["activo"] | c.notificaciones.activo;
  strlcpy(c.notificaciones.servicio, notif["servicio"] | c.notificaciones.servicio,
          sizeof(c.notificaciones.servicio));
  strlcpy(c.notificaciones.destino, notif["destino"] | c.notificaciones.destino,
          sizeof(c.notificaciones.destino));
}

bool cargarConfiguracion(Configuracion &c) {
  aplicarDefectos(c);

  if (!LittleFS.begin()) {
    Serial.println("LittleFS: no monta, lo formateo");
    if (!LittleFS.format() || !LittleFS.begin()) {
      Serial.println("LittleFS: fallo al formatear, sigo con los valores por defecto");
      return false;
    }
  }

  File f = LittleFS.open(CONFIG_FILE_PATH, "r");
  if (!f) {
    Serial.println("config.json no existe, guardo los valores por defecto");
    guardarConfiguracion(c);
    return false;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) {
    Serial.printf("config.json no válido (%s), guardo los valores por defecto\n", err.c_str());
    guardarConfiguracion(c);
    return false;
  }

  configuracionDesdeJson(doc, c);
  return true;
}

bool guardarConfiguracion(const Configuracion &c) {
  JsonDocument doc;
  configuracionAJson(c, doc);

  // Se escribe en un temporal y se renombra: un corte a mitad de escritura
  // no deja un config.json a medias
  const char *tmp = CONFIG_FILE_PATH ".tmp";
  File f = LittleFS.open(tmp, "w");
  if (!f) {
    Serial.println("No se pudo abrir config.json para escritura");
    return false;
  }
  bool ok = serializeJson(doc, f) > 0;
  f.close();
  if (!ok) return false;
  return LittleFS.rename(tmp, CONFIG_FILE_PATH);  // lfs_rename sustituye el destino de forma atómica
}
