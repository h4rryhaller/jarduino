// Prueba de la tapa de Jarduino: LCD (0x27) + 5 botones (0x20).
// SDA -> D2 (GPIO4), SCL -> D1 (GPIO5). Tapa alimentada a 5 V.
// Librería LCD: la LiquidCrystal_I2C que viene dentro de LCDBigNumbers (ArminJo),
// la misma que usa la caja. Es "solo cabeceras": hay que incluir el .hpp, que
// trae la implementación; con el .h solo compila y luego falla al enlazar.

#include <Wire.h>
#include <LiquidCrystal_I2C.hpp>

const uint8_t ADDR_LCD     = 0x27;
const uint8_t ADDR_BOTONES = 0x20;
const unsigned long ANTIRREBOTE_MS = 30;

LiquidCrystal_I2C lcd(ADDR_LCD, 16, 2);

struct Boton { uint8_t bit; const char *nombre; };
const Boton BOTONES[] = {
  // bit = P del PCF8574; entre paréntesis, pin del conector de 16
  // Medido de nuevo tras recolocar los botones (2026-10-03);
  // esta mochila no sigue el orden habitual
  {4, "arriba"},     // P4 (6)
  {6, "abajo"},      // P6
  {5, "izquierda"},  // P5 (5)
  {0, "derecha"},    // P0 (13)
  {2, "centro"}      // P2 (11)
};

uint8_t lecturaAnterior = 0xFF;
uint8_t estadoEstable   = 0xFF;
unsigned long ultimoCambio = 0;
unsigned int pulsaciones = 0;

uint8_t leerBotones() {
  if (Wire.requestFrom(ADDR_BOTONES, (uint8_t)1) != 1) return 0xFF;
  // P3 se ignora: es el transistor de retroiluminación y siempre se lee a 0
  return Wire.read() | (1 << 3);
}

void mostrar(const char *nombre) {
  lcd.setCursor(0, 1);
  lcd.print("                ");
  lcd.setCursor(0, 1);
  lcd.print(nombre);

  char cuenta[4];
  snprintf(cuenta, sizeof(cuenta), "%3u", pulsaciones % 1000);
  lcd.setCursor(13, 1);
  lcd.print(cuenta);
}

void setup() {
  Serial.begin(115200);
  Wire.begin(4, 5);  // SDA=D2, SCL=D1

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Jarduino: tapa");
  mostrar("pulsa un boton");

  // Todo a 1 = entradas con pull-up débil interno
  Wire.beginTransmission(ADDR_BOTONES);
  Wire.write(0xFF);
  if (Wire.endTransmission() != 0) {
    Serial.println("No responde la placa de botones (0x20)");
    mostrar("sin botones 0x20");
  }
}

void loop() {
  uint8_t lectura = leerBotones();

  if (lectura != lecturaAnterior) {
    lecturaAnterior = lectura;
    ultimoCambio = millis();
  }

  // Solo se acepta un cambio si la lectura lleva estable ANTIRREBOTE_MS
  if (lectura != estadoEstable && millis() - ultimoCambio >= ANTIRREBOTE_MS) {
    for (const Boton &b : BOTONES) {
      bool antes = !(estadoEstable & (1 << b.bit));
      bool ahora = !(lectura & (1 << b.bit));
      if (ahora && !antes) {
        pulsaciones++;
        char texto[16];
        snprintf(texto, sizeof(texto), "P%u %s", b.bit, b.nombre);
        Serial.printf("%s pulsado\n", texto);
        mostrar(texto);
      }
    }
    uint8_t asignados = 0;
    for (const Boton &b : BOTONES) asignados |= 1 << b.bit;
    uint8_t nuevos = (estadoEstable & ~lectura) & ~asignados;
    for (uint8_t bit = 0; bit < 8; bit++) {
      if (nuevos & (1 << bit)) {
        char texto[16];
        snprintf(texto, sizeof(texto), "P%u ?", bit);
        Serial.printf("%s pulsado\n", texto);
        mostrar(texto);
      }
    }
    estadoEstable = lectura;
  }
  delay(5);
}
