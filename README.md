# Jarduino

Sistema de riego automático de 4 zonas con ESP8266 (NodeMCU), control por relés, horarios persistentes, API HTTP y una LCD local con menú de 5 pulsadores.

## Estado actual

**Hay un prototipo antiguo real** (relés, ESP8266, RTC en perfboard, sin LCD, PCF8574 ni botones de menú), que se está revisando módulo a módulo desde el 2026-09-21 en vez de empezar de cero. Diseño y API confirmados; queda pendiente añadir LCD/PCF8574×2/pulsadores al prototipo, medir presión/caudal, y todo el firmware. Ver [Próximos pasos](#próximos-pasos) y [Preguntas abiertas](#preguntas-abiertas).

Para el detalle completo (golpe de ariete, medición de caudal, lista de la compra, razonamiento de cada decisión) ver [`Jarduino_diseno_y_agua.md`](./Jarduino_diseno_y_agua.md). Este README es el resumen operativo.

Esquema de cableado (alimentación, relés/válvulas por zona, bus I²C y menú): https://claude.ai/artifact/Ur7ANF999WimwJgnRTRw3u (privado, compartir desde el menú de la página si hace falta — pendiente de actualizar la figura del joystick a los 5 pulsadores).

## Hardware

- **ESP8266 NodeMCU.**
- **Alimentación (confirmado sobre el prototipo real, 2026-09-21):** fuente única, un cargador de portátil Dell reaprovechado (19V/3,1A). Un conversor DC-DC **XL4005E1** (ajustable, 5A/30V) lo baja a 5V para el ESP8266, la bobina de los relés y el resto de la lógica. Los 19V, sin pasar por el buck, alimentan también los contactos de los relés hacia las válvulas. Se retira el módulo pequeño de red (100-240VAC→5V/3W) que traía el prototipo — sin cable de tensión de red en la placa.
- **Módulo de 4 relés** con el jumper VCC/JD-VCC **puesto**: con 5A de margen en el XL4005 no hace falta separar bobina y lógica, GND común a todo.
- **Interruptor manual en paralelo** con el contacto de cada relé — permite regar a mano aunque el ESP falle.
- **Detección de estado por zona** con un optoacoplador PC817 sobre la línea hacia cada válvula (válido tanto si abrió el relé como el interruptor manual).
- **RTC DS3231** (módulo ZS-042) — necesario porque el WiFi llega justo a las válvulas. Si se usa una CR2032 no recargable, quitar el diodo/resistencia del circuito de carga del módulo (pensado para pilas recargables).
- **LCD 16×2 con backpack I2C** en `0x27` (de fábrica, sin puentes). Usa la `LiquidCrystal_I2C` de LCDBigNumbers (ArminJo): incluir `<LiquidCrystal_I2C.hpp>`, no el `.h`, o falla al enlazar.
- **Dos PCF8574** (expansores I2C, mismo chip, direcciones distintas por A0-A2): uno en `0x20` (A0-A2 a GND, otra mochila de LCD) con **5 pulsadores sueltos montados en cruz** (arriba, abajo, izquierda, derecha, centro), común a GND; otro en `0x26` (solo A0 a GND, otra mochila de LCD) con las 4 entradas de estado de zona (salidas del módulo de optoacopladores); evitar P3 (transistor de retroiluminación de la mochila). Módulo opto Hailege probado 2026-09-26: salida en colector abierto sin pull-up propio, **activa a nivel bajo** (válvula con tensión → 0; 0,4 V con el pull-up del PCF8574, sin resistencia externa). Zonas 1-4 = P4, P2, P1, P0 (medido 2026-09-27); P3 siempre a 0, se ignora. La placa de botones no sigue el orden habitual de las mochilas; bits medidos en protoboard (2026-09-23): centro = P5, derecha = P4, arriba = P2, izquierda = P1, abajo = P0 (confirmado con los botones soldados, 2026-09-24). Se valoraron y descartaron un joystick pequeño tipo Ardutype (esta caja va en la pared, no en la mano) y uno analógico KY-023 (necesitaría un ADS1115, más complejo y con deriva de centrado).
- Cada electroválvula lleva su **diodo 1N4007 en antiparalelo** (protección del pico de la bobina al cortar). Las electroválvulas instaladas no tienen marca visible (intemperie), funcionan entre 12 y 24 V — dentro del rango de los 19V del cargador Dell — y son **normalmente cerradas (NC)**.

Descartado: LCD Keypad Shield (pines, forma física, botones analógicos a 5 V frente al ESP a 3,3 V) y PIR (no aplica a este proyecto).

## Mapeo de pines

| Señal | Pin ESP8266 | Notas |
|---|---|---|
| I2C SDA | D2 / GPIO4 | Compartido: LCD, DS3231 (`0x68`), PCF8574 botones de menú (`0x20`), PCF8574 zonas (`0x26`), LCD (`0x27`) |
| I2C SCL | D1 / GPIO5 | |
| Relé IN1 (zona 1) | D7 / GPIO13 | Antes en D3 — pin de arranque, corregido 2026-09-21 |
| Relé IN2 (zona 2) | D6 / GPIO12 | Comprobado con polímetro 2026-09-24 |
| Relé IN3 (zona 3) | D5 / GPIO14 | |
| Relé IN4 (zona 4) | D0 / GPIO16 | Comprobado con polímetro 2026-09-24 |

Se evitan deliberadamente GPIO0/2/15 (boot-strapping).

## Zonas y reglas de riego

1. **Huerto de frutales** (naranjos) — goteo, derivaciones de ~15 mm
2. **Huerta** — goteo, derivaciones de ~15 mm
3. **Jardín** — goteo con derivaciones de menor diámetro; presión demasiado alta para regar solo, siempre necesita compañía
4. **Sin asignar** (futuros frutales) — aún no existe; sin restricción por ahora, `activo: false` en `config.json`

Toda la instalación es de **riego por goteo**, alimentada desde la red (no pozo) por tubería principal de 32 mm, con bastante presión — de ahí la importancia del reductor. Hay dos filtros ya instalados por el sedimento del agua; conviene revisar los goteros de vez en cuando para limpiarlos.

El caudal es fuerte, así que normalmente se riegan varias zonas a la vez. Reglas: zonas 1 y 2 pueden ir solas o juntas; el jardín (zona 3) **nunca solo**. Al combinar zonas, el jardín se abre el último y se cierra el primero, con retardos de 3-5 s entre pasos para evitar el golpe de ariete. Si el ESP detecta (por el opto) que alguien ha abierto el jardín a mano en solitario, abre también la zona 1 para acompañarlo.

## Menú y LCD (5 pulsadores)

5 pulsadores sueltos en cruz (arriba, abajo, izquierda, derecha, centro) en vez de un joystick — la caja va fija en la pared, así que unos botones grandes son más fáciles de acertar. Mismo convenio de gestos que Ardutype: arriba/abajo mueven el cursor o cambian valores; derecha entra o avanza; izquierda vuelve atrás; pulsación central corta confirma (antes "OK corto"), pulsación central larga entra al menú o vuelve al principio (antes "OK largo"). En la pantalla de inicio, pulsación central corta sobre la zona resaltada activa o corta el riego manual de esa zona.

## API HTTP (contrato definido, sin implementar)

- `GET/PUT /api/datetime`
- `GET /api/status`
- `GET/PUT /api/status/{n}` — horario: inicio/fin, días (L-D), `activo`
- `POST /api/riego/{n}/iniciar {duracion_minutos}`
- `POST /api/riego/{n}/detener`
- `POST /api/riego/detener-todo`
- `GET /api/sistema`

Cada zona registra `activada_por`: `automatico` | `manual_switch` | `manual_api` | `manual_lcd` (este último añadido con el firmware, 2026-09-27: riego lanzado desde la tapa). Errores como `{"error", "detalle"}`. Persistencia en LittleFS (`config.json`). Sin autenticación — solo accesible en la LAN. Solapes entre zonas permitidos (salvo la regla del jardín). Una web app mínima, servida desde la flash del ESP en `jarduino.local` (mDNS), consumirá esta misma API — pensada para que la use también quien no sea técnico.

## OTA

Se incluye desde el primer firmware (`ArduinoOTA` + subida web en `/update`), con contraseña obligatoria fuera de git. Partición con espacio OTA (p. ej. 4 MB FS / 2 MB OTA / ~1019 KB). El firmware rechaza o aplaza una actualización si hay riego activo, y muestra progreso en la LCD. El ESP8266 no hace rollback automático — los interruptores manuales en paralelo son la red de seguridad si una subida deja el firmware sin arrancar. La primera subida siempre es por cable.

## `config.json`

Formato definido (2026-09-21), ver [`config.json.example`](./config.json.example) y la sección 11 de [`Jarduino_diseno_y_agua.md`](./Jarduino_diseno_y_agua.md#11-formato-de-configjson-2026-09-21) para el detalle de cada campo. Resumen: un objeto por zona (`id`, `nombre`, `activo`, `necesita_compania`, `horario`) más `sistema.notificaciones`. Cualquier token o secreto (ntfy, Telegram) va aparte en `secrets.json`, nunca en `config.json` ni en git.

## Próximos pasos

1. ~~Medir presiones con el manómetro~~ — pospuesto, la fontanería se deja para más adelante.
2. ~~Definir el formato de `config.json`~~ — hecho (2026-09-21).
3. ~~Montar en protoboard el bus I2C de la tapa y verificar direcciones~~ hecho 2026-09-23 (LCD `0x27` + botones `0x20`, sketches `i2c_scanner/` y `prueba_tapa/`). La tapa va entera a **5 V** (la LCD lo necesita): 4 hilos por la bisagra (5V, GND, SDA, SCL). Botones soldados y confirmados 2026-09-24. DS3231 y relés verificados 2026-09-24. Optoacopladores + PCF8574 de zonas (`0x26`) verificados 2026-09-27. En el montaje final (2026-09-30) la mochila de zonas dejó de responder en `0x26`. Causa (encontrada 2026-10-01): **soldadura defectuosa en el VCC de la mochila** (la continuidad salía según por qué lado se midiera). Sola en la protoboard funcionaba, seguramente alimentada a través de los pull-ups de SDA/SCL, pero en el circuito completo fallaba. Arreglada y confirmada con el escáner I²C. Además, la GND de salida del módulo de optos tiene que ir al pin 1 (VSS) del conector de 16 de la mochila; no basta la GND del conector del bus. El 2026-10-01 volvió a desaparecer: a esa mochila se le había partido un pin del conector de 4, al resoldarlo los pines dejaron de conectar con el chip (ni VCC ni SDA/SCL llegan) y hay un SMD con marca de quemado. **Se descarta y se pide una nueva** (propuesta: módulo PCF8574 con jumpers de dirección, A0=0 A1=1 A2=1 → `0x26`, y ajustar `BIT_ZONA` si cambian los bits). Antes de conectarla, revisar el cableado del módulo de optos (GND a GND, ninguna salida a 5 V o 19 V, sin corto VCC-GND). Mientras tanto se puede probar la fase 1 en el banco sin optos: el firmware solo avisa de que la placa de zonas no responde.
4. Firmware (`jarduino/`), por fases, con tareas no bloqueantes y el riego independiente de la red:
   1. **Núcleo sin red** — hecho 2026-09-27, compila sin avisos y las reglas de compañía se han probado en una simulación en el PC; falta probarlo en el banco. `config.json` en LittleFS, RTC, horarios (si arranca a mitad de una ventana, riega lo que queda), relés escalonados con la regla del jardín, optos (interruptor manual, fallo de válvula), pantalla de inicio de la tapa y consola serie (`T`, `R`, `P`, `E`, `C`; ver `consola.h`).
   2. WiFi + NTP + API HTTP.
   3. Menú de la tapa.
   4. OTA y web app.
   5. Notificaciones.

## Mejoras futuras: detección de fugas

Orden recomendado (charla del 2026-09-30):

1. **Caudalímetro** en la tubería general, después de los filtros y antes de la derivación a la abonadora. Antes de comprarlo, medir el caudal con el contador de la compañía (1, 2 y 3 zonas a la vez) para elegir el tamaño; seguramente de 1" (tipo YF-G1 / FS400A), porque el YF-S201 de 1/2" podría estrangular la tubería de 32 mm. Da pulsos, no es I²C: iría por interrupción en **RX/GPIO3** (D3, D4 y D8 son pines de arranque), que queda libre al pasar la consola a la red en la fase 2, con un divisor 10k/20k para bajar los pulsos a 3,3 V. Permite detectar:
   - **Fuga con todo cerrado**: caudal sostenido sin ninguna zona regando. Entre la llave general y las válvulas solo está la abonadora, así que es fiable; al cargarla o vaciarla puede verse una fuga pequeña, por eso esa alarma sería solo un aviso.
   - **Fuga durante el riego**: una zona gasta un 20-30 % más de lo que suele (tubo suelto, gotero roto). Si gasta menos, hay goteros atascados.
   - **Reventón**: el caudal se dispara → cerrar todo y avisar.
   - Límite: por debajo de ~1 L/min el rotor no gira, así que no ve goteos pequeños.
2. **Electroválvula general** al principio de la línea. Hoy "cerrar todo" solo cierra las 4 zonas: un reventón antes de las válvulas no se puede cortar. Se activaría desde la placa de zonas (`0x26`, P5-P7 libres) con un relé más. Además deja la línea sin presión cuando no se riega y permite la prueba de presión.
3. **Sensor de presión**: transductor de 0-1,2 MPa (0,5-4,5 V) con un ADS1115 (`0x48`) a 5 V en el bus I²C, después de los filtros y del reductor. Con la válvula general cerrada, si la presión baja hay una fuga, por pequeña que sea. También sirve para comprobar que una válvula abre, detectar cortes de agua y vigilar el reductor.

Si se añaden más placas al bus I²C, medir las pull-ups: con 4 placas el equivalente puede rondar ya 1,2 kΩ; quitar las que sobren.

## Agua (resumen)

Todas las preguntas de fontanería están respondidas (ver sección 9 de [`Jarduino_diseno_y_agua.md`](./Jarduino_diseno_y_agua.md#9-preguntas-para-mañana)): agua de red, tubería de 32 mm, riego por goteo, ubicación del caudalímetro identificada, y ante un reventón el sistema cierra todo y avisa. Falta solo medir la presión real y montar el caudalímetro — pospuesto.
