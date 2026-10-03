#include <Arduino.h>
#include "config.h"
#include "configuracion.h"
#include "reloj.h"
#include "riego.h"
#include "programador.h"

// Última ventana de riego automático lanzada en cada zona: día (anio*512 +
// mes*32 + dia) y horario. Se guarda también el horario para que, si se cambia
// después de haber regado ese día, la ventana nueva se lance igualmente.
struct Ventana { uint32_t dia; uint16_t inicio, fin; };
static Ventana lanzada[NUM_ZONAS] = {};
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
    const Ventana ventana = {hoy, z.horario.inicio, z.horario.fin};
    const Ventana &l = lanzada[i];
    if (l.dia == ventana.dia && l.inicio == ventana.inicio && l.fin == ventana.fin) continue;
    lanzada[i] = ventana;

    uint32_t restante = (uint32_t)z.horario.fin * 60 - segundoDelDia;
    // Un riego manual más largo que lo que queda de horario no se acorta
    if (riegoRestanteSeg(i + 1) >= restante) continue;
    riegoIniciarZona(i + 1, restante, Origen::Automatico);
  }
}
