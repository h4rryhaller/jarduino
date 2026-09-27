#include <Arduino.h>
#include "config.h"
#include "configuracion.h"
#include "reloj.h"
#include "riego.h"
#include "programador.h"

// Día (anio*512 + mes*32 + dia) en que ya se lanzó el riego automático de cada zona
static uint32_t diaLanzado[NUM_ZONAS] = {};
static int16_t minutoAnterior = -1;

void programadorTick() {
  if (!relojValido()) return;
  const Fecha &f = relojAhora();
  int16_t minuto = f.hora * 60 + f.min;
  if (minuto == minutoAnterior) return;
  minutoAnterior = minuto;

  uint32_t hoy = (uint32_t)f.anio * 512 + f.mes * 32 + f.dia;
  uint8_t diaSem = diaSemana(f);
  uint32_t segundoDelDia = (uint32_t)minuto * 60 + f.seg;

  for (uint8_t i = 0; i < NUM_ZONAS; i++) {
    const ZonaConfig &z = config.zonas[i];
    if (!z.activo || !(z.horario.dias & (1 << diaSem))) continue;
    if (z.horario.fin <= z.horario.inicio) continue;
    if (minuto < z.horario.inicio || minuto >= z.horario.fin) continue;
    if (diaLanzado[i] == hoy) continue;
    diaLanzado[i] = hoy;

    uint32_t restante = (uint32_t)z.horario.fin * 60 - segundoDelDia;
    // Un riego manual más largo que lo que queda de horario no se acorta
    if (riegoRestanteSeg(i + 1) >= restante) continue;
    riegoIniciarZona(i + 1, restante, Origen::Automatico);
  }
}
