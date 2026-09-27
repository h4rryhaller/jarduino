#ifndef RIEGO_H
#define RIEGO_H

#include <Arduino.h>

// Motor de riego. Las peticiones (horario, tapa, consola, API) solo dicen qué
// zonas se quieren abiertas; riegoTick() mueve los relés de uno en uno, con
// PASO_VALVULAS_MS entre maniobras, respetando la regla de compañía:
//   - una zona con necesita_compania nunca queda abierta sin alguna de sus
//     compañeras: si hace falta, se abre la primera de la lista para acompañarla
//     (también si la zona se abrió a mano con su interruptor);
//   - al abrir, primero las compañeras y después la zona que las necesita;
//     al cerrar, al revés;
//   - se abre antes de cerrar, para que el agua siempre tenga salida.

enum class Origen : uint8_t { Ninguno, Automatico, ManualSwitch, ManualApi, ManualLcd };
const char *origenTexto(Origen o);  // el valor de "activada_por" en la API

struct EstadoZona {
  bool pedida;        // alguien pidió regar esta zona y aún no ha terminado
  bool porCompania;   // abierta solo para acompañar a otra zona
  bool rele;          // estado real del relé
  bool tension;       // el opto ve tensión en la válvula (relé o interruptor)
  bool falloValvula;  // relé activado sin tensión en la válvula
  Origen activadaPor;
  uint32_t inicioMs, duracionMs;  // válidos si pedida
};

void riegoIniciar();  // lo primero de setup(): deja los relés apagados
void riegoTick();

// n = 1..NUM_ZONAS. Si la zona ya estaba pedida, se sustituye la duración.
bool riegoIniciarZona(uint8_t n, uint32_t segundos, Origen origen);
void riegoDetenerZona(uint8_t n);
void riegoDetenerTodo();

const EstadoZona &riegoEstado(uint8_t n);
uint32_t riegoRestanteSeg(uint8_t n);  // 0 si no está pedida
bool riegoZonaAbierta(uint8_t n);      // relé o interruptor manual
bool riegoHayAgua();                   // alguna zona abierta o pendiente de maniobra
bool riegoOptosResponden();

#endif
