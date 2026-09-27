#include <Wire.h>
#include "config.h"
#include "rtc.h"

static uint8_t bcd2dec(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }
static uint8_t dec2bcd(uint8_t v) { return ((v / 10) << 4) | (v % 10); }

static bool leerRegistro(uint8_t reg, uint8_t &valor) {
  Wire.beginTransmission(ADDR_RTC);
  Wire.write(reg);
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom((uint8_t)ADDR_RTC, (uint8_t)1) != 1) return false;
  valor = Wire.read();
  return true;
}

static bool escribirRegistro(uint8_t reg, uint8_t valor) {
  Wire.beginTransmission(ADDR_RTC);
  Wire.write(reg);
  Wire.write(valor);
  return Wire.endTransmission() == 0;
}

bool rtcLeer(Fecha &f) {
  Wire.beginTransmission(ADDR_RTC);
  Wire.write(0x00);
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom((uint8_t)ADDR_RTC, (uint8_t)7) != 7) return false;
  f.seg  = bcd2dec(Wire.read() & 0x7F);
  f.min  = bcd2dec(Wire.read());
  f.hora = bcd2dec(Wire.read() & 0x3F);
  Wire.read();  // día de la semana del chip: se calcula a partir de la fecha
  f.dia  = bcd2dec(Wire.read());
  f.mes  = bcd2dec(Wire.read() & 0x1F);
  f.anio = 2000 + bcd2dec(Wire.read());
  return fechaValida(f);
}

bool rtcEscribir(const Fecha &f) {
  if (!fechaValida(f)) return false;
  Wire.beginTransmission(ADDR_RTC);
  Wire.write(0x00);
  Wire.write(dec2bcd(f.seg));
  Wire.write(dec2bcd(f.min));
  Wire.write(dec2bcd(f.hora));
  Wire.write(diaSemana(f) + 1);
  Wire.write(dec2bcd(f.dia));
  Wire.write(dec2bcd(f.mes));
  Wire.write(dec2bcd(f.anio - 2000));
  if (Wire.endTransmission() != 0) return false;

  // Borra el aviso OSF ("se paró el oscilador")
  uint8_t estado;
  if (!leerRegistro(0x0F, estado)) return false;
  return escribirRegistro(0x0F, estado & 0x7F);
}

bool rtcPerdioHora() {
  uint8_t estado;
  if (!leerRegistro(0x0F, estado)) return false;
  return estado & 0x80;
}

bool fechaValida(const Fecha &f) {
  static const uint8_t diasMes[12] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (f.anio < 2000 || f.anio > 2099) return false;
  if (f.mes < 1 || f.mes > 12) return false;
  if (f.dia < 1 || f.dia > diasMes[f.mes - 1]) return false;
  if (f.mes == 2 && f.dia == 29 && f.anio % 4 != 0) return false;
  return f.hora < 24 && f.min < 60 && f.seg < 60;
}

uint8_t diaSemana(const Fecha &f) {
  // Algoritmo de Sakamoto: 0 = domingo; se pasa a 0 = lunes
  static const uint8_t t[12] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  uint16_t y = f.anio - (f.mes < 3);
  uint8_t domingo0 = (y + y / 4 - y / 100 + y / 400 + t[f.mes - 1] + f.dia) % 7;
  return (domingo0 + 6) % 7;
}
