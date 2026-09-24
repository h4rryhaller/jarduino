// Prueba de RTC (DS3231) y relés de Jarduino, usando la tapa (LCD + botones).
// SDA -> D2 (GPIO4), SCL -> D1 (GPIO5). Monitor serie a 115200.
//
// LCD línea 1: hora del RTC.  Línea 2: las 4 zonas, p. ej. ">1X  2.  3.  4. "
//   '>' = zona seleccionada, 'X' = relé activado, '.' = apagado.
// Botones: izquierda/derecha eligen zona, centro activa/desactiva,
//          abajo apaga todas.
// Por seguridad, cada relé se apaga solo a los AUTO_APAGADO_MS.
//
// Poner en hora por el monitor serie (fin de línea "Nueva línea"):
//   T2026-09-24 18:30:00

#include <Wire.h>
#include <LiquidCrystal_I2C.hpp>  // la de LCDBigNumbers: .hpp, no .h

const uint8_t ADDR_LCD     = 0x27;
const uint8_t ADDR_BOTONES = 0x20;
const uint8_t ADDR_RTC     = 0x68;

// Relés: IN1..IN4. El módulo típico de 4 relés se activa con LOW;
// si al arrancar se encienden todos, cambiar RELE_ON a HIGH.
// Zona n -> relé n del módulo (cableado real comprobado 2026-09-24)
const uint8_t PIN_RELE[4] = {13, 12, 14, 16};  // D7, D6, D5, D0
const uint8_t RELE_ON  = LOW;
const uint8_t RELE_OFF = !RELE_ON;
const unsigned long AUTO_APAGADO_MS = 30000;

// Bits de la placa de botones (medidos en la placa real, 2026-09-24)
const uint8_t BIT_ARRIBA = 2, BIT_ABAJO = 0, BIT_IZQ = 1, BIT_DER = 4, BIT_CENTRO = 5;
const unsigned long ANTIRREBOTE_MS = 30;

LiquidCrystal_I2C lcd(ADDR_LCD, 16, 2);

bool releOn[4] = {false, false, false, false};
unsigned long releDesde[4];
uint8_t seleccion = 0;

uint8_t lecturaAnterior = 0xFF, estadoEstable = 0xFF;
unsigned long ultimoCambio = 0, ultimoRefresco = 0;

// ---------- DS3231 ----------

struct Fecha { uint16_t anio; uint8_t mes, dia, hora, min, seg; };

uint8_t bcd2dec(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }
uint8_t dec2bcd(uint8_t v) { return ((v / 10) << 4) | (v % 10); }

bool rtcLeer(Fecha &f) {
  Wire.beginTransmission(ADDR_RTC);
  Wire.write(0x00);
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom(ADDR_RTC, (uint8_t)7) != 7) return false;
  f.seg  = bcd2dec(Wire.read() & 0x7F);
  f.min  = bcd2dec(Wire.read());
  f.hora = bcd2dec(Wire.read() & 0x3F);  // modo 24 h
  Wire.read();                           // día de la semana, no se usa
  f.dia  = bcd2dec(Wire.read());
  f.mes  = bcd2dec(Wire.read() & 0x1F);
  f.anio = 2000 + bcd2dec(Wire.read());
  return true;
}

void rtcEscribir(const Fecha &f) {
  Wire.beginTransmission(ADDR_RTC);
  Wire.write(0x00);
  Wire.write(dec2bcd(f.seg));
  Wire.write(dec2bcd(f.min));
  Wire.write(dec2bcd(f.hora));
  Wire.write(1);
  Wire.write(dec2bcd(f.dia));
  Wire.write(dec2bcd(f.mes));
  Wire.write(dec2bcd(f.anio - 2000));
  Wire.endTransmission();

  // Borra el aviso OSF ("se paró el oscilador") del registro de estado
  Wire.beginTransmission(ADDR_RTC);
  Wire.write(0x0F);
  Wire.endTransmission();
  Wire.requestFrom(ADDR_RTC, (uint8_t)1);
  uint8_t estado = Wire.read();
  Wire.beginTransmission(ADDR_RTC);
  Wire.write(0x0F);
  Wire.write(estado & 0x7F);
  Wire.endTransmission();
}

// true si el RTC perdió la hora (sin pila o pila agotada)
bool rtcPerdioHora() {
  Wire.beginTransmission(ADDR_RTC);
  Wire.write(0x0F);
  if (Wire.endTransmission() != 0) return false;
  Wire.requestFrom(ADDR_RTC, (uint8_t)1);
  return Wire.read() & 0x80;
}

void leerSerie() {
  static char buf[32];
  static uint8_t n = 0;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r') continue;
    if (c != '\n') {
      if (n < sizeof(buf) - 1) buf[n++] = c;
      continue;
    }
    buf[n] = '\0';
    n = 0;
    Fecha f;
    unsigned a, me, d, h, mi, s;
    if (sscanf(buf, "T%u-%u-%u %u:%u:%u", &a, &me, &d, &h, &mi, &s) == 6) {
      f = {(uint16_t)a, (uint8_t)me, (uint8_t)d, (uint8_t)h, (uint8_t)mi, (uint8_t)s};
      rtcEscribir(f);
      Serial.println("RTC puesto en hora");
    } else {
      Serial.println("Formato: T2026-09-24 18:30:00");
    }
  }
}

// ---------- Relés ----------

void rele(uint8_t i, bool on) {
  releOn[i] = on;
  releDesde[i] = millis();
  digitalWrite(PIN_RELE[i], on ? RELE_ON : RELE_OFF);
  Serial.printf("Zona %u %s\n", i + 1, on ? "ON" : "OFF");
}

// ---------- Botones y pantalla ----------

uint8_t leerBotones() {
  if (Wire.requestFrom(ADDR_BOTONES, (uint8_t)1) != 1) return 0xFF;
  return Wire.read();
}

bool recienPulsado(uint8_t antes, uint8_t ahora, uint8_t bit) {
  return (antes & (1 << bit)) && !(ahora & (1 << bit));
}

void pintar() {
  Fecha f;
  char linea[17];
  lcd.setCursor(0, 0);
  if (rtcLeer(f)) {
    snprintf(linea, sizeof(linea), "%02u:%02u:%02u %02u/%02u%c",
             f.hora, f.min, f.seg, f.dia, f.mes, rtcPerdioHora() ? '!' : ' ');
  } else {
    snprintf(linea, sizeof(linea), "RTC no responde ");
  }
  lcd.print(linea);

  lcd.setCursor(0, 1);
  for (uint8_t i = 0; i < 4; i++) {
    lcd.print(i == seleccion ? '>' : ' ');
    lcd.print(i + 1);
    lcd.print(releOn[i] ? 'X' : '.');
    lcd.print(' ');
  }
}

void setup() {
  // Relés en OFF antes de configurarlos como salida, para que no
  // hagan clic al arrancar
  for (uint8_t i = 0; i < 4; i++) {
    digitalWrite(PIN_RELE[i], RELE_OFF);
    pinMode(PIN_RELE[i], OUTPUT);
  }

  Serial.begin(115200);
  Wire.begin(4, 5);  // SDA=D2, SCL=D1

  lcd.init();
  lcd.backlight();

  Wire.beginTransmission(ADDR_BOTONES);
  Wire.write(0xFF);  // entradas con pull-up débil
  Wire.endTransmission();

  Fecha f;
  if (!rtcLeer(f)) {
    Serial.println("El RTC (0x68) no responde");
  } else {
    Serial.printf("RTC: %04u-%02u-%02u %02u:%02u:%02u\n",
                  f.anio, f.mes, f.dia, f.hora, f.min, f.seg);
    if (rtcPerdioHora())
      Serial.println("El RTC perdio la hora ('!' en la LCD). Ponlo en hora: T2026-09-24 18:30:00");
  }
}

void loop() {
  leerSerie();

  uint8_t lectura = leerBotones();
  if (lectura != lecturaAnterior) {
    lecturaAnterior = lectura;
    ultimoCambio = millis();
  }
  if (lectura != estadoEstable && millis() - ultimoCambio >= ANTIRREBOTE_MS) {
    if (recienPulsado(estadoEstable, lectura, BIT_IZQ)) seleccion = (seleccion + 3) % 4;
    if (recienPulsado(estadoEstable, lectura, BIT_DER)) seleccion = (seleccion + 1) % 4;
    if (recienPulsado(estadoEstable, lectura, BIT_CENTRO)) rele(seleccion, !releOn[seleccion]);
    if (recienPulsado(estadoEstable, lectura, BIT_ABAJO))
      for (uint8_t i = 0; i < 4; i++) if (releOn[i]) rele(i, false);
    estadoEstable = lectura;
    pintar();
  }

  for (uint8_t i = 0; i < 4; i++) {
    if (releOn[i] && millis() - releDesde[i] >= AUTO_APAGADO_MS) {
      Serial.printf("Zona %u: apagado automatico\n", i + 1);
      rele(i, false);
      pintar();
    }
  }

  if (millis() - ultimoRefresco >= 500) {
    ultimoRefresco = millis();
    pintar();
  }
  delay(5);
}
