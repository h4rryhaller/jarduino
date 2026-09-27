#include <Wire.h>
#include "config.h"
#include "configuracion.h"
#include "riego.h"

static EstadoZona zonas[NUM_ZONAS];
static uint32_t ultimaManiobraMs = 0;
static bool hayManiobraPrevia = false;
static uint32_t releCambioMs[NUM_ZONAS];

// Lectura de los optos con antirrebote
static bool optosOk = false;
static uint8_t optoCrudoAnterior = 0xFF;
static uint32_t optoCambioMs = 0;
static uint32_t ultimaLecturaOptoMs = 0;

const char *origenTexto(Origen o) {
  switch (o) {
    case Origen::Automatico:   return "automatico";
    case Origen::ManualSwitch: return "manual_switch";
    case Origen::ManualApi:    return "manual_api";
    case Origen::ManualLcd:    return "manual_lcd";
    default:                   return "ninguno";
  }
}

static uint8_t mascaraCompania(uint8_t i) { return config.zonas[i].compania; }
static bool quiereAbierta(uint8_t i) { return zonas[i].pedida || zonas[i].porCompania; }
static bool abierta(uint8_t i) { return zonas[i].rele || zonas[i].tension; }

// Tensión en la válvula con el relé apagado: está abierto el interruptor manual.
// Justo después de apagar el relé el opto tarda un poco en soltar.
static bool interruptorManual(uint8_t i) {
  return zonas[i].tension && !zonas[i].rele &&
         millis() - releCambioMs[i] > MARGEN_OPTO_MS;
}

static uint8_t bitMasBajo(uint8_t m) { return m & -m; }

static void ponerRele(uint8_t i, bool on) {
  zonas[i].rele = on;
  releCambioMs[i] = millis();
  digitalWrite(PIN_RELE[i], on ? RELE_ON : RELE_OFF);
  Serial.printf("Zona %u: rele %s\n", i + 1, on ? "ON" : "OFF");
}

void riegoIniciar() {
  // Relés en OFF antes de configurarlos como salida, para que no hagan clic
  for (uint8_t i = 0; i < NUM_ZONAS; i++) {
    digitalWrite(PIN_RELE[i], RELE_OFF);
    pinMode(PIN_RELE[i], OUTPUT);
    zonas[i] = {};
    releCambioMs[i] = 0;
  }
}

bool riegoIniciarZona(uint8_t n, uint32_t segundos, Origen origen) {
  if (n < 1 || n > NUM_ZONAS || segundos == 0) return false;
  if (segundos > DURACION_MAX_MIN * 60UL) segundos = DURACION_MAX_MIN * 60UL;
  EstadoZona &z = zonas[n - 1];
  z.pedida = true;
  z.activadaPor = origen;
  z.inicioMs = millis();
  z.duracionMs = segundos * 1000;
  Serial.printf("Zona %u: riego %lu s (%s)\n", n, (unsigned long)segundos, origenTexto(origen));
  return true;
}

void riegoDetenerZona(uint8_t n) {
  if (n < 1 || n > NUM_ZONAS) return;
  if (zonas[n - 1].pedida) Serial.printf("Zona %u: detenida\n", n);
  zonas[n - 1].pedida = false;
}

void riegoDetenerTodo() {
  for (uint8_t n = 1; n <= NUM_ZONAS; n++) riegoDetenerZona(n);
}

const EstadoZona &riegoEstado(uint8_t n) { return zonas[n - 1]; }

uint32_t riegoRestanteSeg(uint8_t n) {
  const EstadoZona &z = zonas[n - 1];
  if (!z.pedida) return 0;
  uint32_t pasado = millis() - z.inicioMs;
  return pasado >= z.duracionMs ? 0 : (z.duracionMs - pasado + 999) / 1000;
}

bool riegoZonaAbierta(uint8_t n) { return abierta(n - 1); }

bool riegoHayAgua() {
  for (uint8_t i = 0; i < NUM_ZONAS; i++)
    if (abierta(i) || quiereAbierta(i)) return true;
  return false;
}

bool riegoOptosResponden() { return optosOk; }

static void leerOptos() {
  if (millis() - ultimaLecturaOptoMs < 20) return;
  ultimaLecturaOptoMs = millis();

  if (Wire.requestFrom((uint8_t)ADDR_ZONAS, (uint8_t)1) != 1) {
    if (optosOk) Serial.println("La placa de zonas (0x26) no responde");
    optosOk = false;
    return;
  }
  if (!optosOk) Serial.println("Placa de zonas (0x26) OK");
  optosOk = true;

  uint8_t crudo = Wire.read();
  if (crudo != optoCrudoAnterior) {
    optoCrudoAnterior = crudo;
    optoCambioMs = millis();
    return;
  }
  if (millis() - optoCambioMs < OPTO_ESTABLE_MS) return;

  for (uint8_t i = 0; i < NUM_ZONAS; i++) {
    bool tension = !(crudo & (1 << BIT_ZONA[i]));  // activa a nivel bajo
    if (tension != zonas[i].tension) {
      zonas[i].tension = tension;
      Serial.printf("Zona %u: %s tension\n", i + 1, tension ? "con" : "sin");
      // Un cierre cuenta como maniobra: si alguien cierra el jardín con su
      // interruptor, su compañera espera PASO_VALVULAS_MS para cerrarse. Una
      // apertura no, para acompañar cuanto antes a un jardín abierto a mano.
      if (!tension) {
        ultimaManiobraMs = millis();
        hayManiobraPrevia = true;
      }
    }
  }
}

static void comprobarValvulas() {
  for (uint8_t i = 0; i < NUM_ZONAS; i++) {
    EstadoZona &z = zonas[i];
    bool fallo = optosOk && z.rele && !z.tension && millis() - releCambioMs[i] > FALLO_VALVULA_MS;
    if (fallo && !z.falloValvula) Serial.printf("Zona %u: rele activado y sin tension en la valvula\n", i + 1);
    z.falloValvula = fallo;
  }
}

// Recalcula pedida/porCompania/activadaPor a partir de las peticiones, los
// tiempos y lo que ven los optos.
static void normalizar() {
  uint8_t pedidas = 0, manuales = 0;
  for (uint8_t i = 0; i < NUM_ZONAS; i++) {
    EstadoZona &z = zonas[i];
    if (z.pedida && millis() - z.inicioMs >= z.duracionMs) {
      z.pedida = false;
      Serial.printf("Zona %u: fin del riego\n", i + 1);
    }
    if (z.pedida) pedidas |= 1 << i;
    if (interruptorManual(i)) manuales |= 1 << i;
  }

  // Compañeras necesarias: para cada zona que necesita compañía y se quiere
  // abierta (o está abierta a mano) sin ninguna compañera pedida o abierta a
  // mano, se conserva la que ya la acompañaba o, si no, la primera de su lista.
  uint8_t acompanantes = 0;
  for (uint8_t i = 0; i < NUM_ZONAS; i++) {
    uint8_t mascara = mascaraCompania(i);
    if (!mascara || !((pedidas | manuales) & (1 << i))) continue;
    if (mascara & (pedidas | manuales)) continue;
    uint8_t actuales = 0;
    for (uint8_t c = 0; c < NUM_ZONAS; c++)
      if (zonas[c].porCompania) actuales |= 1 << c;
    uint8_t elegida = bitMasBajo(mascara & actuales);
    if (!elegida) elegida = bitMasBajo(mascara);
    if (!(acompanantes & elegida)) {
      uint8_t c = __builtin_ctz(elegida);
      if (!zonas[c].porCompania)
        Serial.printf("Zona %u sin compania: abro la zona %u para acompanarla\n", i + 1, c + 1);
      if (!zonas[c].pedida) zonas[c].activadaPor = zonas[i].activadaPor;
    }
    acompanantes |= elegida;
  }

  for (uint8_t i = 0; i < NUM_ZONAS; i++) {
    EstadoZona &z = zonas[i];
    z.porCompania = acompanantes & (1 << i);
    if (manuales & (1 << i)) {
      if (!z.pedida) z.activadaPor = Origen::ManualSwitch;
    } else if (!quiereAbierta(i) && !abierta(i)) {
      z.activadaPor = Origen::Ninguno;
    }
  }
}

// ¿Se puede cerrar el relé de la compañera c sin dejar sola a ninguna zona
// abierta que la necesite?
static bool puedeCerrarCompanera(uint8_t c) {
  for (uint8_t d = 0; d < NUM_ZONAS; d++) {
    uint8_t mascara = mascaraCompania(d);
    if (!(mascara & (1 << c)) || !abierta(d)) continue;
    bool otra = false;
    for (uint8_t c2 = 0; c2 < NUM_ZONAS; c2++)
      if (c2 != c && (mascara & (1 << c2)) && abierta(c2)) otra = true;
    if (!otra) return false;
  }
  return true;
}

static bool tieneCompaneraAbierta(uint8_t d) {
  uint8_t mascara = mascaraCompania(d);
  for (uint8_t c = 0; c < NUM_ZONAS; c++)
    if ((mascara & (1 << c)) && zonas[c].rele) return true;
  return false;
}

// Una maniobra como mucho por llamada, en orden de prioridad. Devuelve la
// zona a conmutar o -1.
static int siguienteManiobra() {
  // 1. cerrar una zona que necesita compañía
  for (uint8_t i = 0; i < NUM_ZONAS; i++)
    if (mascaraCompania(i) && zonas[i].rele && !quiereAbierta(i)) return i;
  // 2. abrir una zona que puede regar sola
  for (uint8_t i = 0; i < NUM_ZONAS; i++)
    if (!mascaraCompania(i) && !zonas[i].rele && quiereAbierta(i)) return i;
  // 3. cerrar una zona que puede regar sola, si no deja a nadie sin compañía
  for (uint8_t i = 0; i < NUM_ZONAS; i++)
    if (!mascaraCompania(i) && zonas[i].rele && !quiereAbierta(i) && puedeCerrarCompanera(i)) return i;
  // 4. abrir una zona que necesita compañía, con alguna compañera ya abierta
  for (uint8_t i = 0; i < NUM_ZONAS; i++)
    if (mascaraCompania(i) && !zonas[i].rele && quiereAbierta(i) && tieneCompaneraAbierta(i)) return i;
  return -1;
}

void riegoTick() {
  leerOptos();
  normalizar();
  comprobarValvulas();

  if (hayManiobraPrevia && millis() - ultimaManiobraMs < PASO_VALVULAS_MS) return;
  int i = siguienteManiobra();
  if (i < 0) return;
  ponerRele(i, !zonas[i].rele);
  ultimaManiobraMs = millis();
  hayManiobraPrevia = true;
}
