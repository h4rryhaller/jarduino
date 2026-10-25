#ifndef REGISTRO_H
#define REGISTRO_H

#include <Arduino.h>

// Log del firmware: lo que se escribe en `registro` sale por el puerto serie
// igual que antes y además se guarda, con la hora del RTC delante de cada
// línea, en un búfer circular de REGISTRO_BYTES (las líneas más antiguas se
// pierden). La API lo sirve en GET /api/log y la web app lo muestra.
// La consola serie (consola.cpp) sigue escribiendo en Serial: sus respuestas
// no son eventos y no van al log.
class Registro : public Print {
 public:
  size_t write(uint8_t c) override;
  size_t write(const uint8_t *buf, size_t n) override;
  String texto() const;  // de la línea más antigua completa a la más nueva
  using Print::write;

 private:
  void guardar(char c);
  void guardarMarca();
  bool inicioLinea = true;
};

extern Registro registro;

#endif
