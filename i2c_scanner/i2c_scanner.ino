// Escáner I2C para Jarduino + prueba de los 5 botones de la tapa.
// SDA -> D2 (GPIO4), SCL -> D1 (GPIO5). Monitor serie a 115200.

#include <Wire.h>

const uint8_t ADDR_BOTONES = 0x20;

struct Boton { uint8_t bit; const char *nombre; };
const Boton BOTONES[] = {
  // bit = P del PCF8574; entre paréntesis, pin del conector de 16
  // Medido de nuevo tras recolocar los botones (2026-10-03);
  // esta mochila no sigue el orden habitual
  {4, "arriba"},     // P4 (6)
  {6, "abajo"},      // P6 (4)
  {5, "izquierda"},  // P5 (5)
  {0, "derecha"},    // P0 (13)
  {2, "centro"}      // P2 (11)
};

const char *identificar(uint8_t addr) {
  switch (addr) {
    case 0x20: return "PCF8574 botones";
    case 0x26: return "PCF8574 zonas";
    case 0x27: return "PCF8574 LCD";
    case 0x57: return "EEPROM AT24C32 (modulo del RTC)";
    case 0x68: return "RTC DS3231";
    default:   return "desconocido";
  }
}

bool botonesPresentes = false;
uint8_t ultimoEstado = 0xFF;

void escanear() {
  Serial.println("\nEscaneando bus I2C...");
  int encontrados = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  0x%02X  %s\n", addr, identificar(addr));
      encontrados++;
      if (addr == ADDR_BOTONES) botonesPresentes = true;
    }
  }
  Serial.printf("%d dispositivo(s)\n", encontrados);
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Wire.begin(4, 5);  // SDA=D2, SCL=D1
  escanear();

  if (botonesPresentes) {
    // Todo a 1 = entradas con pull-up débil interno
    Wire.beginTransmission(ADDR_BOTONES);
    Wire.write(0xFF);
    Wire.endTransmission();
    Serial.println("\nPulsa los botones (pulsado = 0). Reset para volver a escanear.");
  }
}

void loop() {
  if (!botonesPresentes) return;

  if (Wire.requestFrom(ADDR_BOTONES, (uint8_t)1) != 1) return;
  uint8_t estado = Wire.read();

  if (estado != ultimoEstado) {
    for (const Boton &b : BOTONES) {
      bool antes = !(ultimoEstado & (1 << b.bit));
      bool ahora = !(estado & (1 << b.bit));
      if (ahora && !antes) Serial.printf("%s pulsado\n", b.nombre);
      if (!ahora && antes) Serial.printf("%s soltado\n", b.nombre);
    }
    ultimoEstado = estado;
  }
  delay(20);  // antirrebote basto, suficiente para la prueba
}
