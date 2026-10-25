#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>
#include <Updater.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>

#include "config.h"
#include "riego.h"
#include "tapa.h"
#include "api.h"
#include "ota.h"

#if __has_include("secretos.h")
#include "secretos.h"
#else
#error "Falta secretos.h: copia secretos.h.example como secretos.h y pon una clave propia"
#endif

static int ultimoPorcentaje = -1;

static void pintarProgreso(const char *titulo, int porcentaje) {
  if (porcentaje == ultimoPorcentaje) return;
  ultimoPorcentaje = porcentaje;
  char linea[LCD_COLUMNAS + 1];
  snprintf(linea, sizeof(linea), "%3d %%", porcentaje);
  tapaPintarAhora(titulo, linea);
}

// ---- ArduinoOTA (desde el IDE) ----

static void iniciarArduinoOta() {
  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);

  ArduinoOTA.onStart([]() {
    Serial.println("OTA: empieza la subida");
    ultimoPorcentaje = -1;
    pintarProgreso("Actualizando", 0);
  });
  ArduinoOTA.onProgress([](unsigned int hecho, unsigned int total) {
    pintarProgreso("Actualizando", total ? hecho * 100 / total : 0);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("OTA: terminada, reiniciando");
    tapaPintarAhora("Actualizado", "Reiniciando...");
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA: error %u\n", error);
    char texto[LCD_COLUMNAS + 1];
    snprintf(texto, sizeof(texto), "OTA fallida (%u)", error);
    tapaMensaje(texto, 5000);
  });

  // Sin el mDNS propio de ArduinoOTA: ya lo lleva red.cpp (MDNS.update en redTick)
  ArduinoOTA.begin(false);
  MDNS.enableArduino(8266, true);
  Serial.println("OTA: ArduinoOTA en " HOSTNAME ".local:8266");
}

// ---- Subida web en /update ----

// Resultado de la subida web: errorWeb vacío si va bien
static int codigoWeb = 200;
static const char *tipoErrorWeb = "";
static String errorWeb;

static void fallarWeb(int codigo, const char *tipo, const String &detalle) {
  codigoWeb = codigo;
  tipoErrorWeb = tipo;
  errorWeb = detalle;
  if (codigo != 401) Serial.printf("OTA web: %s\n", detalle.c_str());
}

static void enviarErrorJson(int code, const char *error, const String &detalle) {
  apiServidor().send(code, "application/json",
                     String("{\"error\":\"") + error + "\",\"detalle\":\"" + detalle + "\"}");
}

static bool autorizado() {
  return apiServidor().authenticate(OTA_USUARIO, OTA_PASSWORD);
}

static void getUpdate() {
  ESP8266WebServer &server = apiServidor();
  if (!autorizado()) return server.requestAuthentication();
  server.send(200, "text/html",
              "<!doctype html><meta charset=utf-8><title>Jarduino OTA</title>"
              "<form method=post enctype=multipart/form-data>"
              "<p>Firmware (.bin): <input type=file name=firmware accept=.bin required>"
              "<p><button>Subir</button></form>");
}

static void postUpdateFin() {
  ESP8266WebServer &server = apiServidor();
  if (!autorizado()) return server.requestAuthentication();
  if (errorWeb.length()) {
    enviarErrorJson(codigoWeb, tipoErrorWeb, errorWeb);
    if (codigoWeb == 500) tapaMensaje("OTA fallida", 5000);
    return;
  }
  if (!Update.isFinished()) return enviarErrorJson(400, "sin_firmware", "falta el campo firmware (.bin)");
  server.send(200, "application/json", "{\"resultado\":\"ok\",\"detalle\":\"reiniciando\"}");
  Serial.println("OTA web: terminada, reiniciando");
  tapaPintarAhora("Actualizado", "Reiniciando...");
  delay(500);
  ESP.restart();
}

static void postUpdateDatos() {
  ESP8266WebServer &server = apiServidor();
  HTTPUpload &subida = server.upload();

  if (subida.status == UPLOAD_FILE_START) {
    errorWeb = "";
    if (!autorizado()) return fallarWeb(401, "sin_autorizar", "usuario o clave incorrectos");
    if (riegoHayAgua()) return fallarWeb(409, "riego_activo", "hay agua abierta; espera a que termine");
    Serial.printf("OTA web: subiendo %s\n", subida.filename.c_str());
    WiFiUDP::stopAll();
    uint32_t libre = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
    if (!Update.begin(libre)) return fallarWeb(500, "ota_fallida", Update.getErrorString());
    ultimoPorcentaje = -1;
    tapaPintarAhora("Actualizando", "web");
  } else if (subida.status == UPLOAD_FILE_WRITE) {
    if (errorWeb.length()) return;
    if (Update.write(subida.buf, subida.currentSize) != subida.currentSize)
      return fallarWeb(500, "ota_fallida", Update.getErrorString());
    // Sin el tamaño total a mano: se muestran los KB recibidos
    char linea[LCD_COLUMNAS + 1];
    snprintf(linea, sizeof(linea), "web %u KB", (unsigned)(subida.totalSize / 1024));
    tapaPintarAhora("Actualizando", linea);
  } else if (subida.status == UPLOAD_FILE_END) {
    if (errorWeb.length()) return;
    if (!Update.end(true)) fallarWeb(500, "ota_fallida", Update.getErrorString());
  } else if (subida.status == UPLOAD_FILE_ABORTED) {
    Update.end();
    if (!errorWeb.length()) fallarWeb(500, "ota_fallida", "subida cancelada");
  }
}

void otaIniciar() {
  iniciarArduinoOta();
  apiServidor().on("/update", HTTP_GET, getUpdate);
  apiServidor().on("/update", HTTP_POST, postUpdateFin, postUpdateDatos);
}

void otaTick() {
  // Con agua no se atiende al IDE: su invitación queda sin respuesta.
  if (!riegoHayAgua()) ArduinoOTA.handle();
}
