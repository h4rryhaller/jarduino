// Prueba aislada de la placa de zonas (PCF8574 en 0x26) con un NodeMCU aparte.
// Solo 4 cables entre el NodeMCU y la mochila:
//   3V3 o VIN(5 V) -> VCC   GND -> GND   D2 (GPIO4) -> SDA   D1 (GPIO5) -> SCL
// Monitor serie a 115200.
//
// Cada 2 s comprueba el estado de las líneas SDA/SCL y escanea el bus entero,
// así se puede tocar o repasar soldaduras sin pulsar reset. Si aparece la
// 0x26, muestra también el estado de las 4 entradas de zona cuando cambian.

#include <Wire.h>

const uint8_t ADDR_ZONAS = 0x26;
const uint8_t BIT_ZONA[4] = {4, 2, 1, 0};  // zonas 1-4 = P4, P2, P1, P0

bool zonasPresente = false;
uint8_t ultimoPuerto = 0;
bool puertoLeido = false;
unsigned long ultimoEscaneo = 0;

const char *estadoBus(uint8_t s) {
  switch (s) {
    case I2C_OK:                      return "OK";
    case I2C_SCL_HELD_LOW:            return "SCL pegada a 0 (corto a GND o cable)";
    case I2C_SCL_HELD_LOW_AFTER_READ: return "SCL pegada a 0 tras una lectura";
    case I2C_SDA_HELD_LOW:            return "SDA pegada a 0 (corto a GND o cable)";
    case I2C_SDA_HELD_LOW_AFTER_INIT: return "SDA pegada a 0 tras iniciar";
    default:                          return "desconocido";
  }
}

void escanear() {
  Serial.printf("\nLineas I2C: %s\n", estadoBus(Wire.status()));
  int encontrados = 0;
  bool antes = zonasPresente;
  zonasPresente = false;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  0x%02X%s\n", addr, addr == ADDR_ZONAS ? "  <- placa de zonas" : "");
      encontrados++;
      if (addr == ADDR_ZONAS) zonasPresente = true;
    }
  }
  Serial.printf("%d dispositivo(s)%s\n", encontrados,
                zonasPresente ? "" : " -- la 0x26 NO responde");

  if (zonasPresente && !antes) {
    Wire.beginTransmission(ADDR_ZONAS);
    Wire.write(0xFF);  // entradas con pull-up débil
    Wire.endTransmission();
    puertoLeido = false;
  }
}

void leerZonas() {
  if (Wire.requestFrom(ADDR_ZONAS, (uint8_t)1) != 1) return;
  uint8_t puerto = Wire.read();
  if (puertoLeido && puerto == ultimoPuerto) return;
  ultimoPuerto = puerto;
  puertoLeido = true;

  Serial.print("Puerto P7..P0 = ");
  for (int b = 7; b >= 0; b--) Serial.print((puerto >> b) & 1);
  Serial.print("   ");
  // Opto activo a nivel bajo: 0 = válvula con tensión. Sin el módulo de
  // optos conectado, todas deben salir "sin" (pull-up a 1)
  for (uint8_t z = 0; z < 4; z++)
    Serial.printf("Z%u:%s ", z + 1, (puerto & (1 << BIT_ZONA[z])) ? "sin" : "CON");
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\nPrueba de la placa de zonas (0x26)");
  Wire.begin(4, 5);  // SDA=D2, SCL=D1
}

void loop() {
  if (millis() - ultimoEscaneo >= 2000) {
    ultimoEscaneo = millis();
    escanear();
  }
  if (zonasPresente) leerZonas();
  delay(20);
}
