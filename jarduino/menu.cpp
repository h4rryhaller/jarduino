#include <Arduino.h>
#include "config.h"
#include "configuracion.h"
#include "reloj.h"
#include "rtc.h"
#include "riego.h"
#include "red.h"
#include "tapa.h"
#include "menu.h"

enum class Pantalla : uint8_t { Principal, Zonas, Zona, FechaHora, Sistema };

enum { P_ZONAS, P_PARAR_TODO, P_FECHA, P_SISTEMA, NUM_PRINCIPAL };
static const char *const PRINCIPAL[NUM_PRINCIPAL] = {"Zonas", "Parar todo", "Fecha y hora", "Sistema"};

enum { Z_INICIO, Z_FIN, Z_DIAS, Z_REGAR, Z_PARAR, NUM_OPCIONES_ZONA };
static const char *const OPCIONES_ZONA[NUM_OPCIONES_ZONA] = {"Inicio", "Fin", "Dias", "Regar ahora", "Parar"};

enum { S_IP, S_WIFI, S_RSSI, S_NTP, S_ENCENDIDO, S_FIRMWARE, NUM_SISTEMA };

static const char LETRAS_DIAS[] = "LMXJVSD";

static bool activo = false;
static Pantalla pantalla = Pantalla::Principal;
static uint8_t opcionPrincipal = 0, zona = 0, opcionZona = 0, opcionSistema = 0;
static uint32_t ultimaTeclaMs = 0;

// Edición: campos numéricos con su rango; arriba/abajo dan la vuelta
struct Campo { uint16_t valor, min, max; };
static bool editando = false;
static Campo campos[7];
static uint8_t numCampos = 0, campo = 0;

static void empezarEdicion(uint8_t n) {
  numCampos = n;
  campo = 0;
  editando = true;
}

static void ponerCampo(uint8_t i, uint16_t valor, uint16_t min, uint16_t max) {
  campos[i] = {constrain(valor, min, max), min, max};
}

void menuAbrir() {
  activo = true;
  pantalla = Pantalla::Principal;
  opcionPrincipal = 0;
  editando = false;
  ultimaTeclaMs = millis();
}

bool menuActivo() { return activo; }

static void cerrar() {
  activo = false;
  editando = false;
}

static void guardar() {
  tapaMensaje(guardarConfiguracion(config) ? "Guardado" : "Error al guardar");
}

static void editarOpcionZona() {
  const Horario &h = config.zonas[zona].horario;
  switch (opcionZona) {
    case Z_INICIO:
    case Z_FIN: {
      uint16_t minutos = opcionZona == Z_INICIO ? h.inicio : h.fin;
      ponerCampo(0, minutos / 60, 0, 23);
      ponerCampo(1, minutos % 60, 0, 59);
      empezarEdicion(2);
      break;
    }
    case Z_DIAS:
      for (uint8_t d = 0; d < 7; d++) ponerCampo(d, (h.dias >> d) & 1, 0, 1);
      empezarEdicion(7);
      break;
    case Z_REGAR:
      ponerCampo(0, DURACION_MANUAL_MIN, 1, DURACION_MAX_MIN);
      empezarEdicion(1);
      break;
  }
}

static void confirmarZona() {
  Horario &h = config.zonas[zona].horario;
  char texto[24];  // cabe en 16: zona <= 4 y minutos <= DURACION_MAX_MIN
  switch (opcionZona) {
    case Z_INICIO:
      h.inicio = campos[0].valor * 60 + campos[1].valor;
      guardar();
      break;
    case Z_FIN:
      h.fin = campos[0].valor * 60 + campos[1].valor;
      guardar();
      break;
    case Z_DIAS:
      h.dias = 0;
      for (uint8_t d = 0; d < 7; d++)
        if (campos[d].valor) h.dias |= 1 << d;
      guardar();
      break;
    case Z_REGAR:
      riegoIniciarZona(zona + 1, campos[0].valor * 60UL, Origen::ManualLcd);
      snprintf(texto, sizeof(texto), "Zona %u: %u min", zona + 1, campos[0].valor);
      tapaMensaje(texto);
      break;
  }
}

static void editarFecha() {
  Fecha f = relojAhora();
  if (!relojResponde() || !fechaValida(f)) f = {2026, 1, 1, 0, 0, 0};
  ponerCampo(0, f.dia, 1, 31);
  ponerCampo(1, f.mes, 1, 12);
  ponerCampo(2, f.anio - 2000, 0, 99);
  ponerCampo(3, f.hora, 0, 23);
  ponerCampo(4, f.min, 0, 59);
  empezarEdicion(5);
}

static void confirmarFecha() {
  Fecha f = {(uint16_t)(2000 + campos[2].valor), (uint8_t)campos[1].valor, (uint8_t)campos[0].valor,
             (uint8_t)campos[3].valor, (uint8_t)campos[4].valor, 0};
  if (!fechaValida(f)) {
    tapaMensaje("Fecha no valida");
    editando = true;  // se sigue editando para corregirla
    return;
  }
  tapaMensaje(rtcEscribir(f) ? "Hora guardada" : "RTC no responde");
}

static void teclaEditando(Tecla t) {
  Campo &c = campos[campo];
  switch (t) {
    case Tecla::Arriba: c.valor = c.valor >= c.max ? c.min : c.valor + 1; break;
    case Tecla::Abajo:  c.valor = c.valor <= c.min ? c.max : c.valor - 1; break;
    case Tecla::Izq:
      if (campo > 0) campo--;
      else editando = false;  // cancelar
      break;
    case Tecla::Der:
      if (campo < numCampos - 1) campo++;
      break;
    case Tecla::Centro:
      editando = false;
      if (pantalla == Pantalla::Zona) confirmarZona();
      else if (pantalla == Pantalla::FechaHora) confirmarFecha();
      break;
    default: break;
  }
}

static void siguiente(uint8_t &opcion, uint8_t total, Tecla t) {
  if (t == Tecla::Abajo) opcion = (opcion + 1) % total;
  else opcion = (opcion + total - 1) % total;
}

void menuTecla(Tecla t) {
  ultimaTeclaMs = millis();
  if (t == Tecla::CentroLargo) return cerrar();
  if (editando) return teclaEditando(t);

  bool vertical = t == Tecla::Arriba || t == Tecla::Abajo;
  bool entrar = t == Tecla::Der || t == Tecla::Centro;
  char texto[LCD_COLUMNAS + 1];

  switch (pantalla) {
    case Pantalla::Principal:
      if (vertical) siguiente(opcionPrincipal, NUM_PRINCIPAL, t);
      else if (t == Tecla::Izq) cerrar();
      else if (opcionPrincipal == P_PARAR_TODO) {
        // Las acciones solo con centro; derecha solo entra en submenús
        if (t == Tecla::Centro) {
          riegoDetenerTodo();
          tapaMensaje("Todo parado");
        }
      } else if (entrar) {
        if (opcionPrincipal == P_ZONAS) pantalla = Pantalla::Zonas;
        else if (opcionPrincipal == P_FECHA) pantalla = Pantalla::FechaHora;
        else { pantalla = Pantalla::Sistema; opcionSistema = 0; }
      }
      break;

    case Pantalla::Zonas:
      if (vertical) siguiente(zona, NUM_ZONAS, t);
      else if (t == Tecla::Izq) pantalla = Pantalla::Principal;
      else if (entrar) { pantalla = Pantalla::Zona; opcionZona = 0; }
      break;

    case Pantalla::Zona:
      if (vertical) siguiente(opcionZona, NUM_OPCIONES_ZONA, t);
      else if (t == Tecla::Izq) pantalla = Pantalla::Zonas;
      else if (opcionZona == Z_PARAR) {
        if (t == Tecla::Centro) {
          riegoDetenerZona(zona + 1);
          snprintf(texto, sizeof(texto), "Zona %u: parada", zona + 1);
          tapaMensaje(texto);
        }
      } else if (entrar) {
        editarOpcionZona();
      }
      break;

    case Pantalla::FechaHora:
      if (t == Tecla::Izq) pantalla = Pantalla::Principal;
      else if (entrar) editarFecha();
      break;

    case Pantalla::Sistema:
      if (vertical) siguiente(opcionSistema, NUM_SISTEMA, t);
      else if (t == Tecla::Izq) pantalla = Pantalla::Principal;
      break;
  }
}

void menuTick() {
  if (activo && millis() - ultimaTeclaMs >= MENU_INACTIVIDAD_MS) cerrar();
}

// "1 Huerto     15m": número, nombre y estado de la zona (16 columnas)
static void tituloZona(uint8_t i, char *buf, size_t tam) {
  char nombre[LCD_COLUMNAS + 1], estado[8];
  textoLcd(config.zonas[i].nombre, nombre, sizeof(nombre));
  estadoCortoZona(i + 1, estado, sizeof(estado));
  snprintf(buf, tam, "%u %-10.10s %.3s", i + 1, nombre, estado);
}

static void lineaZona(char *buf, size_t tam, int8_t &cursor) {
  const Horario &h = config.zonas[zona].horario;
  const char *etiqueta = OPCIONES_ZONA[opcionZona];
  switch (opcionZona) {
    case Z_INICIO:
    case Z_FIN: {
      uint16_t minutos = opcionZona == Z_INICIO ? h.inicio : h.fin;
      uint16_t hh = editando ? campos[0].valor : minutos / 60;
      uint16_t mm = editando ? campos[1].valor : minutos % 60;
      snprintf(buf, tam, "%-11s%02u:%02u", etiqueta, hh, mm);
      if (editando) cursor = campo == 0 ? 12 : 15;
      break;
    }
    case Z_DIAS: {
      char dias[8];
      for (uint8_t d = 0; d < 7; d++) {
        bool on = editando ? campos[d].valor : (h.dias >> d) & 1;
        dias[d] = on ? LETRAS_DIAS[d] : '-';
      }
      dias[7] = '\0';
      snprintf(buf, tam, "%-9s%s", etiqueta, dias);
      if (editando) cursor = 9 + campo;
      break;
    }
    case Z_REGAR:
      if (editando) {
        snprintf(buf, tam, "Regar %3u min", campos[0].valor);
        cursor = 8;
      } else {
        strlcpy(buf, etiqueta, tam);
      }
      break;
    default:
      strlcpy(buf, etiqueta, tam);
  }
}

static void lineaSistema(char *buf, size_t tam) {
  switch (opcionSistema) {
    case S_IP:
      if (redConectada()) snprintf(buf, tam, "IP %s", redIp().c_str());
      else strlcpy(buf, "IP: sin WiFi", tam);
      break;
    case S_WIFI:
      if (redConectada()) snprintf(buf, tam, "WiFi %s", redSsid().c_str());
      else strlcpy(buf, "WiFi: no conecta", tam);
      break;
    case S_RSSI:
      if (redConectada()) snprintf(buf, tam, "RSSI %d dBm", redRssi());
      else strlcpy(buf, "RSSI: -", tam);
      break;
    case S_NTP:
      snprintf(buf, tam, "NTP: %s", ntpSincronizado() ? "sincronizado" : "no");
      break;
    case S_ENCENDIDO: {
      uint32_t min = millis() / 60000;
      if (min >= 24 * 60) snprintf(buf, tam, "Encendido %lud%02luh", (unsigned long)(min / 1440), (unsigned long)(min / 60 % 24));
      else snprintf(buf, tam, "Encendido %luh%02lum", (unsigned long)(min / 60), (unsigned long)(min % 60));
      break;
    }
    case S_FIRMWARE:
      snprintf(buf, tam, "Fw %s", __DATE__);
      break;
  }
}

void menuPantalla(char *linea0, char *linea1, size_t tam, int8_t &cursor) {
  cursor = -1;
  switch (pantalla) {
    case Pantalla::Principal:
      strlcpy(linea0, "Menu", tam);
      snprintf(linea1, tam, ">%s", PRINCIPAL[opcionPrincipal]);
      break;
    case Pantalla::Zonas:
      strlcpy(linea0, "Zonas", tam);
      tituloZona(zona, linea1, tam);
      break;
    case Pantalla::Zona:
      tituloZona(zona, linea0, tam);
      lineaZona(linea1, tam, cursor);
      break;
    case Pantalla::FechaHora: {
      strlcpy(linea0, "Fecha y hora", tam);
      const Fecha &f = relojAhora();
      if (editando)
        snprintf(linea1, tam, "%02u/%02u/%02u   %02u:%02u", campos[0].valor, campos[1].valor,
                 campos[2].valor, campos[3].valor, campos[4].valor);
      else if (relojResponde())
        snprintf(linea1, tam, "%02u/%02u/%02u   %02u:%02u", f.dia, f.mes, f.anio % 100, f.hora, f.min);
      else
        strlcpy(linea1, "RTC no responde", tam);
      static const int8_t COLUMNAS[] = {1, 4, 7, 12, 15};
      if (editando) cursor = COLUMNAS[campo];
      break;
    }
    case Pantalla::Sistema:
      strlcpy(linea0, "Sistema", tam);
      lineaSistema(linea1, tam);
      break;
  }
}
