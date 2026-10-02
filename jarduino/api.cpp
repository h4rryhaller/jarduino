#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <uri/UriBraces.h>
#include <ArduinoJson.h>

#include "config.h"
#include "configuracion.h"
#include "riego.h"
#include "reloj.h"
#include "rtc.h"
#include "red.h"
#include "api.h"

static ESP8266WebServer server(HTTP_PORT);

// ---- Utilidades de respuesta ----
static void enviarJson(int code, JsonDocument &doc) {
  String out;
  serializeJson(doc, out);
  server.send(code, "application/json", out);
}

static void enviarError(int code, const char *error, const char *detalle) {
  JsonDocument doc;
  doc["error"] = error;
  doc["detalle"] = detalle;
  enviarJson(code, doc);
}

// Devuelve 1..NUM_ZONAS, o 0 si el parámetro de ruta no es una zona válida.
static uint8_t zonaDeRuta() {
  int n = server.pathArg(0).toInt();
  return (n >= 1 && n <= NUM_ZONAS) ? (uint8_t)n : 0;
}

// Config de la zona + su estado de riego en vivo, en un objeto.
static void zonaEstadoAJson(uint8_t n, JsonObject obj) {
  zonaAJson(config.zonas[n - 1], obj);
  const EstadoZona &z = riegoEstado(n);
  JsonObject e = obj["estado"].to<JsonObject>();
  e["rele"] = z.rele;
  e["tension"] = z.tension;
  e["pedida"] = z.pedida;
  e["por_compania"] = z.porCompania;
  e["fallo_valvula"] = z.falloValvula;
  e["activada_por"] = origenTexto(z.activadaPor);
  e["restante_seg"] = riegoRestanteSeg(n);
  e["abierta"] = riegoZonaAbierta(n);
}

static void datetimeAJson(JsonObject obj) {
  const Fecha &f = relojAhora();
  char buf[20];
  snprintf(buf, sizeof(buf), "%04u-%02u-%02u %02u:%02u:%02u", f.anio, f.mes, f.dia, f.hora, f.min,
           f.seg);
  obj["datetime"] = buf;
  obj["rtc_responde"] = relojResponde();
  obj["reloj_valido"] = relojValido();
  obj["ntp_sincronizado"] = ntpSincronizado();
}

// ---- Manejadores ----
static void getStatus() {
  JsonDocument doc;
  JsonArray zonas = doc["zonas"].to<JsonArray>();
  for (uint8_t n = 1; n <= NUM_ZONAS; n++) zonaEstadoAJson(n, zonas.add<JsonObject>());
  enviarJson(200, doc);
}

static void getStatusZona() {
  uint8_t n = zonaDeRuta();
  if (!n) return enviarError(404, "zona_no_encontrada", "La zona debe estar entre 1 y 4");
  JsonDocument doc;
  zonaEstadoAJson(n, doc.to<JsonObject>());
  enviarJson(200, doc);
}

static void putStatusZona() {
  uint8_t n = zonaDeRuta();
  if (!n) return enviarError(404, "zona_no_encontrada", "La zona debe estar entre 1 y 4");

  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain")))
    return enviarError(400, "json_no_valido", "El cuerpo no es JSON válido");

  ZonaConfig z = config.zonas[n - 1];
  if (!zonaDesdeJson(doc.as<JsonObjectConst>(), z))
    return enviarError(400, "campos_no_validos", "Algún campo del horario o la zona no es válido");

  config.zonas[n - 1] = z;
  if (!guardarConfiguracion(config))
    return enviarError(500, "no_guardado", "No se pudo escribir config.json");

  JsonDocument resp;
  zonaEstadoAJson(n, resp.to<JsonObject>());
  enviarJson(200, resp);
}

static void getDatetime() {
  JsonDocument doc;
  datetimeAJson(doc.to<JsonObject>());
  enviarJson(200, doc);
}

static void putDatetime() {
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain")))
    return enviarError(400, "json_no_valido", "El cuerpo no es JSON válido");

  const char *s = doc["datetime"] | "";
  unsigned a, me, d, h, mi, sg;
  if (sscanf(s, "%u-%u-%u %u:%u:%u", &a, &me, &d, &h, &mi, &sg) != 6)
    return enviarError(400, "formato_no_valido", "Usa \"datetime\": \"YYYY-MM-DD HH:MM:SS\"");

  Fecha f = {(uint16_t)a, (uint8_t)me, (uint8_t)d, (uint8_t)h, (uint8_t)mi, (uint8_t)sg};
  if (!rtcEscribir(f))
    return enviarError(400, "fecha_no_valida", "Fecha fuera de rango o el RTC no responde");

  // Se responde con la fecha recién escrita (relojTick tiene throttle de 500 ms
  // y el espejo en memoria aún no la reflejaría).
  char buf[20];
  snprintf(buf, sizeof(buf), "%04u-%02u-%02u %02u:%02u:%02u", f.anio, f.mes, f.dia, f.hora, f.min,
           f.seg);
  JsonDocument resp;
  resp["datetime"] = buf;
  resp["rtc_responde"] = true;
  resp["reloj_valido"] = true;  // rtcEscribir borra el aviso OSF
  resp["ntp_sincronizado"] = ntpSincronizado();
  enviarJson(200, resp);
}

static void postIniciar() {
  uint8_t n = zonaDeRuta();
  if (!n) return enviarError(404, "zona_no_encontrada", "La zona debe estar entre 1 y 4");

  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain")))
    return enviarError(400, "json_no_valido", "El cuerpo no es JSON válido");

  int minutos = doc["duracion_minutos"] | 0;
  if (minutos < 1 || minutos > DURACION_MAX_MIN) {
    char det[64];
    snprintf(det, sizeof(det), "duracion_minutos debe estar entre 1 y %d", DURACION_MAX_MIN);
    return enviarError(400, "duracion_no_valida", det);
  }

  if (!riegoIniciarZona(n, (uint32_t)minutos * 60UL, Origen::ManualApi))
    return enviarError(400, "no_iniciado", "No se pudo iniciar el riego de esa zona");

  JsonDocument resp;
  zonaEstadoAJson(n, resp.to<JsonObject>());
  enviarJson(200, resp);
}

static void postDetener() {
  uint8_t n = zonaDeRuta();
  if (!n) return enviarError(404, "zona_no_encontrada", "La zona debe estar entre 1 y 4");
  riegoDetenerZona(n);
  JsonDocument resp;
  zonaEstadoAJson(n, resp.to<JsonObject>());
  enviarJson(200, resp);
}

static void postDetenerTodo() {
  riegoDetenerTodo();
  getStatus();
}

static void getSistema() {
  JsonDocument doc;
  doc["version"] = config.version;
  doc["uptime_s"] = millis() / 1000;
  doc["heap_libre"] = ESP.getFreeHeap();
  doc["hay_agua"] = riegoHayAgua();
  doc["optos_responden"] = riegoOptosResponden();

  datetimeAJson(doc["reloj"].to<JsonObject>());

  JsonObject w = doc["wifi"].to<JsonObject>();
  w["conectado"] = redConectada();
  w["ssid"] = redSsid();
  w["ip"] = redIp();
  w["rssi"] = redRssi();

  JsonObject nt = doc["notificaciones"].to<JsonObject>();
  nt["activo"] = config.notificaciones.activo;
  nt["servicio"] = config.notificaciones.servicio;
  nt["destino"] = config.notificaciones.destino;

  enviarJson(200, doc);
}

void apiIniciar() {
  server.on("/api/status", HTTP_GET, getStatus);
  server.on(UriBraces("/api/status/{}"), HTTP_GET, getStatusZona);
  server.on(UriBraces("/api/status/{}"), HTTP_PUT, putStatusZona);

  server.on("/api/datetime", HTTP_GET, getDatetime);
  server.on("/api/datetime", HTTP_PUT, putDatetime);

  server.on("/api/riego/detener-todo", HTTP_POST, postDetenerTodo);
  server.on(UriBraces("/api/riego/{}/iniciar"), HTTP_POST, postIniciar);
  server.on(UriBraces("/api/riego/{}/detener"), HTTP_POST, postDetener);

  server.on("/api/sistema", HTTP_GET, getSistema);

  server.onNotFound([]() { enviarError(404, "no_encontrado", server.uri().c_str()); });

  server.begin();
  Serial.println("API HTTP en el puerto 80");
}

void apiTick() {
  server.handleClient();
}
