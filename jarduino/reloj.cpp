#include <Arduino.h>
#include "reloj.h"

static Fecha ahora;
static bool responde = false, perdioHora = false;
static uint32_t ultimaLecturaMs = 0;
static bool leidoAlgunaVez = false;

void relojTick() {
  if (leidoAlgunaVez && millis() - ultimaLecturaMs < 500) return;
  ultimaLecturaMs = millis();

  bool antes = responde;
  responde = rtcLeer(ahora);
  if (responde) perdioHora = rtcPerdioHora();
  if (responde != antes || !leidoAlgunaVez)
    Serial.println(responde ? "RTC (0x68) OK" : "El RTC (0x68) no responde");
  leidoAlgunaVez = true;
}

bool relojValido() { return responde && !perdioHora; }
bool relojResponde() { return responde; }
const Fecha &relojAhora() { return ahora; }
