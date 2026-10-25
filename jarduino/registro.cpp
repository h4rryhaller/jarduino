#include "config.h"
#include "reloj.h"
#include "registro.h"

Registro registro;

static char bufer[REGISTRO_BYTES];
static size_t inicio = 0, usado = 0;

void Registro::guardar(char c) {
  bufer[(inicio + usado) % REGISTRO_BYTES] = c;
  if (usado < REGISTRO_BYTES) usado++;
  else inicio = (inicio + 1) % REGISTRO_BYTES;
}

// "25 07:00:03 " con hora del RTC, o "+123s " (segundos desde el arranque)
// mientras el RTC no responde o no tiene hora fiable.
void Registro::guardarMarca() {
  char marca[16];
  if (relojResponde() && relojValido()) {
    const Fecha &f = relojAhora();
    snprintf(marca, sizeof(marca), "%02u %02u:%02u:%02u ", f.dia, f.hora, f.min, f.seg);
  } else {
    snprintf(marca, sizeof(marca), "+%lus ", (unsigned long)(millis() / 1000));
  }
  for (const char *p = marca; *p; p++) guardar(*p);
}

size_t Registro::write(uint8_t c) {
  Serial.write(c);
  if (c == '\r') return 1;
  if (inicioLinea) guardarMarca();
  guardar((char)c);
  inicioLinea = c == '\n';
  return 1;
}

size_t Registro::write(const uint8_t *buf, size_t n) {
  for (size_t i = 0; i < n; i++) write(buf[i]);
  return n;
}

String Registro::texto() const {
  String s;
  s.reserve(usado);
  size_t i = 0;
  // Si el búfer ya dio la vuelta, la primera línea está cortada: se salta
  if (usado == REGISTRO_BYTES) {
    while (i < usado && bufer[(inicio + i) % REGISTRO_BYTES] != '\n') i++;
    if (i < usado) i++;
  }
  for (; i < usado; i++) s += bufer[(inicio + i) % REGISTRO_BYTES];
  return s;
}
