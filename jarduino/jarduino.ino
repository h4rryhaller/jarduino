// Jarduino: riego automático de 4 zonas con ESP8266 (NodeMCU).
// Fase 1: todo lo que riega sin red (horarios, reglas de compañía, relés,
// optos, tapa y consola serie). Ver README.md del proyecto.

#include <Wire.h>
#include "config.h"
#include "configuracion.h"
#include "riego.h"
#include "reloj.h"
#include "programador.h"
#include "tapa.h"
#include "consola.h"

void setup() {
  riegoIniciar();  // antes que nada: relés apagados

  Serial.begin(115200);
  Serial.println();
  Serial.println("Jarduino arrancando");

  Wire.begin(PIN_SDA, PIN_SCL);
  riegoIniciarOptos();
  cargarConfiguracion(config);
  tapaIniciar();
  relojTick();

  Serial.println("Listo. Escribe una orden o una linea vacia + E para el estado");
}

void loop() {
  relojTick();
  programadorTick();
  riegoTick();
  tapaTick();
  consolaTick();
}
