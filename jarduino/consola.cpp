#include <Arduino.h>
#include "config.h"
#include "configuracion.h"
#include "reloj.h"
#include "riego.h"
#include "red.h"
#include "consola.h"

static void imprimirEstado(Print &out) {
  const Fecha &f = relojAhora();
  if (relojResponde())
    out.printf("Hora: %04u-%02u-%02u %02u:%02u:%02u%s\n", f.anio, f.mes, f.dia, f.hora, f.min,
                  f.seg, relojValido() ? "" : " (perdio la hora)");
  else
    out.println("Hora: el RTC no responde");
  if (!riegoOptosResponden()) out.println("Optos: la placa de zonas no responde");

  for (uint8_t n = 1; n <= NUM_ZONAS; n++) {
    const EstadoZona &z = riegoEstado(n);
    out.printf("Zona %u %-12s rele=%u tension=%u pedida=%u compania=%u fallo=%u origen=%s restante=%lus\n",
                  n, config.zonas[n - 1].nombre, z.rele, z.tension, z.pedida, z.porCompania,
                  z.falloValvula, origenTexto(z.activadaPor), (unsigned long)riegoRestanteSeg(n));
  }
}

void consolaEjecutar(const char *orden, Print &out) {
  unsigned a, me, d, h, mi, s, n, minutos;
  if (sscanf(orden, "T%u-%u-%u %u:%u:%u", &a, &me, &d, &h, &mi, &s) == 6) {
    Fecha f = {(uint16_t)a, (uint8_t)me, (uint8_t)d, (uint8_t)h, (uint8_t)mi, (uint8_t)s};
    out.println(rtcEscribir(f) ? "RTC puesto en hora" : "Fecha no valida o el RTC no responde");
  } else if (sscanf(orden, "R %u %u", &n, &minutos) == 2) {
    if (!riegoIniciarZona(n, minutos * 60UL, Origen::ManualApi)) out.println("Zona o duracion no validas");
  } else if (sscanf(orden, "P %u", &n) == 1) {
    riegoDetenerZona(n);
  } else if (strcmp(orden, "P") == 0) {
    riegoDetenerTodo();
  } else if (strcmp(orden, "E") == 0) {
    imprimirEstado(out);
  } else if (strcmp(orden, "C") == 0) {
    JsonDocument doc;
    configuracionAJson(config, doc);
    serializeJsonPretty(doc, out);
    out.println();
  } else if (strcmp(orden, "W") == 0) {
    if (redConectada())
      out.printf("WiFi: conectado a \"%s\"  IP %s  RSSI %d dBm  NTP=%s\n", redSsid().c_str(),
                    redIp().c_str(), redRssi(), ntpSincronizado() ? "si" : "no");
    else
      out.printf("WiFi: SIN conexion (SSID guardado: \"%s\")\n", redSsid().c_str());
  } else {
    out.println("Ordenes: T2026-09-24 18:30:00 | R <zona> <min> | P [zona] | E | C | W");
  }
}

void consolaTick() {
  static char buf[40];
  static uint8_t n = 0;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r') continue;
    if (c != '\n') {
      if (n < sizeof(buf) - 1) buf[n++] = c;
      continue;
    }
    buf[n] = '\0';
    n = 0;
    if (buf[0]) consolaEjecutar(buf, Serial);
  }
}
