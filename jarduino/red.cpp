#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiManager.h>
#include <time.h>

#include "config.h"
#include "rtc.h"
#include "red.h"

static bool sincronizado = false;      // el RTC se ha corregido con NTP al menos una vez
static uint32_t ultimoSyncMs = 0;
static bool mdnsOk = false;

// Pone el RTC en hora con la hora local de NTP, si es fiable.
static void sincronizarRtcDesdeNtp() {
  if (!redConectada()) return;
  time_t ahora = time(nullptr);
  if (ahora < 1600000000) return;  // SNTP aún no ha respondido (antes de 2020)

  if (sincronizado && millis() - ultimoSyncMs < RTC_RESYNC_MS) return;

  struct tm lt;
  localtime_r(&ahora, &lt);  // hora local, la TZ ya la aplicó configTime()
  Fecha f = {(uint16_t)(lt.tm_year + 1900), (uint8_t)(lt.tm_mon + 1), (uint8_t)lt.tm_mday,
             (uint8_t)lt.tm_hour, (uint8_t)lt.tm_min, (uint8_t)lt.tm_sec};
  if (rtcEscribir(f)) {
    sincronizado = true;
    ultimoSyncMs = millis();
    Serial.printf("RTC sincronizado por NTP: %04u-%02u-%02u %02u:%02u:%02u\n", f.anio, f.mes, f.dia,
                  f.hora, f.min, f.seg);
  }
}

void redIniciar() {
  WiFi.mode(WIFI_STA);
  WiFi.hostname(HOSTNAME);
  WiFi.setAutoReconnect(true);

  WiFiManager wm;
  // El riego no depende de la red, pero autoConnect() bloquea setup() mientras
  // lo intenta. Acotamos el peor caso: 20 s probando las credenciales guardadas
  // y, si fallan, el portal cautivo con su propio timeout. Mejora futura: abrir
  // el portal solo bajo demanda (pulsación larga en la tapa) para no bloquear
  // nunca el arranque si el router está caído.
  wm.setConnectTimeout(20);
  wm.setConfigPortalTimeout(PORTAL_TIMEOUT_S);
  Serial.println("WiFi: autoConnect (portal \"" AP_SETUP "\" si no hay credenciales)");
  if (wm.autoConnect(AP_SETUP)) {
    Serial.print("WiFi conectado, IP: ");
    Serial.println(WiFi.localIP());
    if (MDNS.begin(HOSTNAME)) {
      MDNS.addService("http", "tcp", HTTP_PORT);
      mdnsOk = true;
      Serial.println("mDNS: http://" HOSTNAME ".local");
    }
  } else {
    Serial.println("WiFi: sin conexion (timeout del portal). Sigo offline con el RTC.");
  }

  // Arranca SNTP con la zona horaria; sincroniza en cuanto haya red.
  configTime(TZ_HORARIO, NTP_SERVER1, NTP_SERVER2);
}

void redTick() {
  if (mdnsOk) MDNS.update();
  sincronizarRtcDesdeNtp();
}

bool redConectada() { return WiFi.status() == WL_CONNECTED; }
int redRssi() { return redConectada() ? WiFi.RSSI() : 0; }
String redIp() { return redConectada() ? WiFi.localIP().toString() : String("0.0.0.0"); }
String redSsid() { return WiFi.SSID(); }
bool ntpSincronizado() { return sincronizado; }
