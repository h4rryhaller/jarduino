#ifndef API_H
#define API_H

// API HTTP REST (solo LAN, sin autenticación). Contrato en el README.
// Reutiliza los helpers JSON de configuracion.* y las funciones de riego.*
void apiIniciar();  // tras redIniciar()
void apiTick();     // atiende peticiones; no bloquea el riego

#endif
