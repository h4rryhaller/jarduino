#include <Wire.h>
#include <LiquidCrystal_I2C.hpp>  // la de LCDBigNumbers: .hpp, no .h
#include "config.h"
#include "configuracion.h"
#include "reloj.h"
#include "riego.h"
#include "tapa.h"

static LiquidCrystal_I2C lcd(ADDR_LCD, LCD_COLUMNAS, LCD_FILAS);

static uint8_t seleccion = 0;
static char pintado[LCD_FILAS][LCD_COLUMNAS + 1];
static char mensaje[LCD_COLUMNAS + 1];
static uint32_t mensajeDesdeMs = 0, mensajeDuracionMs = 0;
static uint32_t ultimoRefrescoMs = 0;

static uint8_t lecturaAnterior = 0xFF, estable = 0xFF;
static uint32_t ultimoCambioMs = 0, ultimaLecturaMs = 0, centroDesdeMs = 0;
static bool largaDisparada = false;

// La ROM de la LCD no tiene tildes: se quitan; la ñ está en 0xEE
static void aLcd(const char *utf8, char *out, size_t tam) {
  size_t n = 0;
  for (const uint8_t *p = (const uint8_t *)utf8; *p && n < tam - 1; p++) {
    if (*p < 0x80) { out[n++] = *p; continue; }
    if (*p == 0xC3 && p[1]) {
      p++;
      static const char tabla[] = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYPsaaaaaaaceeeeiiiidnooooo/ouuuuypy";
      out[n++] = (*p == 0xB1 || *p == 0x91) ? (char)0xEE : tabla[(*p - 0x80) & 0x3F];
      continue;
    }
    while ((p[1] & 0xC0) == 0x80) p++;  // otros caracteres de varios bytes
    out[n++] = '?';
  }
  out[n] = '\0';
}

static char simboloZona(uint8_t n) {
  const EstadoZona &z = riegoEstado(n);
  bool quiere = z.pedida || z.porCompania;
  if (z.falloValvula) return '!';
  if (quiere != z.rele) return '~';
  if (z.tension && !z.rele) return 'M';
  if (z.porCompania && !z.pedida) return 'c';
  if (z.rele) return '*';
  return config.zonas[n - 1].activo ? '.' : '-';
}

static void estadoCorto(uint8_t n, char *buf, size_t tam) {
  const EstadoZona &z = riegoEstado(n);
  if (z.falloValvula) { strlcpy(buf, "err", tam); return; }
  if (z.pedida) {
    uint32_t min = (riegoRestanteSeg(n) + 59) / 60;
    if (min < 100) snprintf(buf, tam, "%2lum", (unsigned long)min);
    else snprintf(buf, tam, "%3lu", (unsigned long)min);
    return;
  }
  if (z.porCompania) { strlcpy(buf, "acp", tam); return; }
  if (z.tension) { strlcpy(buf, "man", tam); return; }
  strlcpy(buf, "off", tam);
}

static void escribirLinea(uint8_t fila, const char *texto) {
  char linea[LCD_COLUMNAS + 1];
  snprintf(linea, sizeof(linea), "%-16s", texto);
  if (strcmp(linea, pintado[fila]) == 0) return;
  strcpy(pintado[fila], linea);
  lcd.setCursor(0, fila);
  lcd.print(linea);
}

static void pintar() {
  char linea[LCD_COLUMNAS + 1];

  if (millis() - mensajeDesdeMs < mensajeDuracionMs) {
    escribirLinea(0, mensaje);
  } else {
    char hora[10], nombre[LCD_COLUMNAS + 1], estado[10];
    const Fecha &f = relojAhora();
    if (relojResponde()) snprintf(hora, sizeof(hora), "%02u:%02u%c", f.hora, f.min, relojValido() ? ' ' : '!');
    else strlcpy(hora, "--:--?", sizeof(hora));
    aLcd(config.zonas[seleccion].nombre, nombre, sizeof(nombre));
    estadoCorto(seleccion + 1, estado, sizeof(estado));
    snprintf(linea, sizeof(linea), "%.6s%-6.6s %.3s", hora, nombre, estado);
    escribirLinea(0, linea);
  }

  size_t n = 0;
  for (uint8_t i = 0; i < NUM_ZONAS; i++) {
    linea[n++] = i == seleccion ? '>' : ' ';
    linea[n++] = '1' + i;
    linea[n++] = simboloZona(i + 1);
    linea[n++] = ' ';
  }
  linea[n] = '\0';
  escribirLinea(1, linea);
}

void tapaMensaje(const char *texto, uint32_t ms) {
  aLcd(texto, mensaje, sizeof(mensaje));
  mensajeDesdeMs = millis();
  mensajeDuracionMs = ms;
  pintar();
}

static void pulsacionCentro() {
  uint8_t n = seleccion + 1;
  const EstadoZona &z = riegoEstado(n);
  char texto[LCD_COLUMNAS + 1];
  if (z.pedida) {
    riegoDetenerZona(n);
    snprintf(texto, sizeof(texto), "Zona %u: parada", n);
  } else if (z.porCompania) {
    snprintf(texto, sizeof(texto), "Acompanando");
  } else if (z.tension && !z.rele) {
    snprintf(texto, sizeof(texto), "Interruptor man.");
  } else {
    riegoIniciarZona(n, DURACION_MANUAL_MIN * 60UL, Origen::ManualLcd);
    snprintf(texto, sizeof(texto), "Zona %u: %u min", n, DURACION_MANUAL_MIN);
  }
  tapaMensaje(texto);
}

static bool bajado(uint8_t antes, uint8_t ahora, uint8_t bit) {
  return (antes & (1 << bit)) && !(ahora & (1 << bit));
}

static bool subido(uint8_t antes, uint8_t ahora, uint8_t bit) {
  return bajado(ahora, antes, bit);
}

static void leerBotones() {
  if (millis() - ultimaLecturaMs < 10) return;
  ultimaLecturaMs = millis();
  if (Wire.requestFrom((uint8_t)ADDR_BOTONES, (uint8_t)1) != 1) return;
  uint8_t lectura = Wire.read();

  if (lectura != lecturaAnterior) {
    lecturaAnterior = lectura;
    ultimoCambioMs = millis();
  }
  if (lectura != estable && millis() - ultimoCambioMs >= ANTIRREBOTE_MS) {
    uint8_t antes = estable;
    estable = lectura;
    mensajeDuracionMs = 0;  // cualquier botón quita el mensaje
    if (bajado(antes, estable, BIT_IZQ)) seleccion = (seleccion + NUM_ZONAS - 1) % NUM_ZONAS;
    if (bajado(antes, estable, BIT_DER)) seleccion = (seleccion + 1) % NUM_ZONAS;
    if (bajado(antes, estable, BIT_CENTRO)) {
      centroDesdeMs = millis();
      largaDisparada = false;
    }
    if (subido(antes, estable, BIT_CENTRO) && !largaDisparada) pulsacionCentro();
    pintar();
  }

  if (!(estable & (1 << BIT_CENTRO)) && !largaDisparada &&
      millis() - centroDesdeMs >= PULSACION_LARGA_MS) {
    largaDisparada = true;
    tapaMensaje("Menu: pendiente");
  }
}

void tapaIniciar() {
  lcd.init();
  lcd.backlight();
  pintado[0][0] = pintado[1][0] = '\0';

  Wire.beginTransmission(ADDR_BOTONES);
  Wire.write(0xFF);  // entradas con pull-up débil
  if (Wire.endTransmission() != 0) Serial.println("La placa de botones (0x20) no responde");
  tapaMensaje("Jarduino", 1500);
}

void tapaTick() {
  leerBotones();
  if (millis() - ultimoRefrescoMs >= 250) {
    ultimoRefrescoMs = millis();
    pintar();
  }
}
