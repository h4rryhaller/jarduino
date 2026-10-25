#ifndef OTA_H
#define OTA_H

// Actualización del firmware por WiFi, de dos formas:
//   - ArduinoOTA: el IDE ve "jarduino" como puerto de red (puerto 8266).
//   - Web: POST /update con autenticación básica (usuario OTA_USUARIO),
//     p. ej. curl -u admin:<clave> -F firmware=@jarduino.ino.bin http://jarduino.local/update
// Las dos usan OTA_PASSWORD de secretos.h (fuera de git, ver secretos.h.example).
//
// Mientras haya agua (riegoHayAgua(): una zona abierta por relé o interruptor,
// o una maniobra pendiente) no se acepta ninguna actualización: la subida
// bloquea el loop y los relés no se podrían mover. La subida web responde 409;
// el IDE solo ve que el ESP no contesta.
void otaIniciar();  // tras apiIniciar()
void otaTick();

#endif
