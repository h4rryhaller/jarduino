#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ---- Bus I2C (SDA=D2/GPIO4, SCL=D1/GPIO5) ----
#define PIN_SDA 4
#define PIN_SCL 5
#define ADDR_LCD     0x27
#define ADDR_BOTONES 0x20
#define ADDR_ZONAS   0x26  // salidas de los optos, activas a nivel bajo
#define ADDR_RTC     0x68

#define NUM_ZONAS 4

// Relés IN1..IN4 = zonas 1..4 (D7, D6, D5, D0; comprobado 2026-09-24).
// Se evitan GPIO0/2/15: un relé que parpadea en el arranque abriría una válvula.
static const uint8_t PIN_RELE[NUM_ZONAS] = {13, 12, 14, 16};
#define RELE_ON  LOW
#define RELE_OFF HIGH

// Bit de la placa de zonas para cada zona. Módulo PCF8574 GERUI (2026-10-02)
// con P0-P7 directos: zona n -> P(n-1). Zona 1=P0, 2=P1, 3=P2, 4=P3.
static const uint8_t BIT_ZONA[NUM_ZONAS] = {0, 1, 2, 3};

// Bits de la placa de botones (medidos de nuevo tras recolocar los botones, 2026-10-03)
#define BIT_ARRIBA 4
#define BIT_ABAJO  6
#define BIT_IZQ    5
#define BIT_DER    0
#define BIT_CENTRO 2
#define ANTIRREBOTE_MS     30
#define PULSACION_LARGA_MS 800

// ---- LCD ----
#define LCD_COLUMNAS 16
#define LCD_FILAS    2

// ---- Riego ----
#define PASO_VALVULAS_MS    4000  // entre dos maniobras de válvula (golpe de ariete)
#define OPTO_ESTABLE_MS     300   // antirrebote de la lectura de los optos
#define FALLO_VALVULA_MS    3000  // relé activado y sin tensión en la válvula
#define MARGEN_OPTO_MS      1000  // lo que tarda el opto en seguir a un cambio de relé
#define DURACION_MANUAL_MIN 15    // riego manual desde la tapa
#define DURACION_MAX_MIN    240   // tope de cualquier riego, venga de donde venga

// ---- Persistencia ----
#define CONFIG_FILE_PATH "/config.json"

// ---- Red (fase 2) ----
#define HTTP_PORT       80
#define HOSTNAME        "jarduino"        // DHCP + mDNS -> jarduino.local
#define AP_SETUP        "Jarduino-setup"  // AP del portal cautivo de WiFiManager
#define PORTAL_TIMEOUT_S 300              // si nadie configura el portal, sigue offline
#define TZ_HORARIO      "CET-1CEST,M3.5.0,M10.5.0/3"  // España peninsular, con cambio de hora
#define NTP_SERVER1     "pool.ntp.org"
#define NTP_SERVER2     "time.nist.gov"
#define RTC_RESYNC_MS   86400000UL        // re-sincroniza el RTC con NTP 1 vez/día

#endif
