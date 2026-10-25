# Jarduino

Sistema de riego automático de 4 zonas con ESP8266 (NodeMCU), control por relés, horarios persistentes, API HTTP y una LCD local con menú de 5 pulsadores.

## Estado actual

**Hardware montado y probado en el banco (2026-10-25)**: se partió del prototipo antiguo (relés, ESP8266, RTC en perfboard), revisado módulo a módulo desde el 2026-09-21, y se le añadieron la LCD, los dos PCF8574, los pulsadores, los optos y la nueva alimentación de 24 V AC. Las 4 zonas encienden su relé y el opto detecta la tensión. Firmware: fases 1-3 hechas (núcleo, WiFi/API y menú). Queda probar con las válvulas y los interruptores manuales en su sitio, medir la WiFi allí, y lo pospuesto (presión, caudal, OTA y web app). Ver [Próximos pasos](#próximos-pasos) y [Preguntas abiertas](#preguntas-abiertas).

Para el detalle completo (golpe de ariete, medición de caudal, lista de la compra, razonamiento de cada decisión) ver [`Jarduino_diseno_y_agua.md`](./Jarduino_diseno_y_agua.md). Este README es el resumen operativo.

Esquema de cableado (alimentación, relés/válvulas por zona, bus I²C y menú): https://claude.ai/artifact/Ur7ANF999WimwJgnRTRw3u (privado, compartir desde el menú de la página si hace falta). Actualizado 2026-10-25 con la alimentación de 24 V AC, los optos y el mapeo actual de zonas y botones.

## Hardware

- **ESP8266 NodeMCU.**
- **Alimentación — rediseñada (2026-10-10, montada 2026-10-18).** Antes: un cargador Dell reaprovechado (19V/3,1A) con un buck **XL4005E1** a 5V para la lógica, y los 19V directos a las válvulas. El Dell murió (4,5 V en vacío) y, al revisar las válvulas antes de conectarlas, resultaron ser de **24 V AC**, así que 19 V DC tampoco les valía (en continua solo las frena la resistencia: 19 V / 43 Ω ≈ 8 W en una bobina de 2 W). Nuevo diseño, dos alimentaciones:
  - **Válvulas:** transformador **24 V AC, 30 VA**, de hierro encapsulado en resina, IP68 y enchufe Schuko (de los de bombas de estanque; 1,33 kg, así que es de 50 Hz y no electrónico). Da 27,5 V AC en vacío. Con la recomendación del fabricante de no pasar del 80 % quedan ~1 A útiles: justo para las 4 zonas con apertura escalonada, sin margen para una futura válvula general. **Fusible de 1,25 A lento en el cable rojo**, antes de repartir a los relés y los interruptores manuales; el negro es el retorno común de las válvulas (y de IN− de los optos), sin fusible. El secundario está aislado y no se une ni a tierra ni a la GND de la lógica.
  - **Lógica:** **cargador de móvil Xiaomi de 5 V / 3 A** (5,1 V medidos) directo a la línea de 5 V, sin el XL4005 (rectificar los 24 V AC daría ~34-38 V, por encima de los 32 V del XL4005). **La GND del NodeMCU tiene que ir unida a la de las placas**, sobre todo con el NodeMCU por USB al PC: el 2026-10-18 el escáner I²C no veía ninguna placa porque el cable de GND estaba en el VIN del NodeMCU; como el cargador está aislado no hubo corto, pero todo el lado del Xiaomi quedaba ~5 V por encima de la GND del ESP.
  - Los relés e interruptores manuales conmutan la alterna igual que antes.
- **Módulo de 4 relés** con el jumper VCC/JD-VCC **puesto**: con los 3 A del cargador no hace falta separar bobina y lógica, GND común a todo.
- **Interruptor manual en paralelo** con el contacto de cada relé — permite regar a mano aunque el ESP falle.
- **Detección de estado por zona** con un optoacoplador PC817 sobre la línea hacia cada válvula (válido tanto si abrió el relé como el interruptor manual). Cableado: IN+ al punto común de relé e interruptor, IN− al retorno común de las válvulas. Con 24 V AC hay que poner un **1N4007 en serie con cada IN+** (ánodo hacia el relé, raya hacia IN+): en el semiciclo negativo el LED del PC817 solo aguanta 6 V en inversa y el diodo bloquea esa tensión (se pueden reutilizar los 1N4007 que se quitan de las válvulas). **No en antiparalelo entre IN+ e IN−**: en el módulo la resistencia queda dentro, así que el diodo puentearía la válvula y cortocircuitaría el transformador en cada semiciclo negativo. El transformador da 27,5 V AC en vacío (~39 V de pico), por encima de los 30 V del módulo Hailege, pero su resistencia de entrada es de 3 kΩ (`302`): ~12 mA de pico por el LED y ~0,12 W en la resistencia (menos que con 30 V DC), así que no hace falta resistencia extra. Además la salida del opto **parpadea a 50 Hz** (el PC817 solo conduce en un semiciclo). Se filtra por firmware, sin condensadores: `leerOptos()` lee cada 5 ms (`OPTO_LECTURA_MS`) y una zona tiene tensión si se ha visto a 0 en una ventana de 40 ms (`OPTO_VENTANA_MS`), con el antirrebote de 300 ms encima. Antes leía cada 20 ms, justo un ciclo de red, y habría muestreado siempre el mismo punto de la onda. Los condensadores solos no valían: con el pull-up débil del PCF8574 (30-300 µA) tardarían más de 1 s en soltar y se confundiría con el interruptor manual (harían falta 10 µF + 10 kΩ a 5 V por zona). **Limitación:** el opto mide la tensión en la caja, no en la válvula (a 2-3 m de cable): un cable cortado, un borne suelto o una bobina quemada siguen leyéndose como "abierta". El aviso de "fallo de válvula" del firmware (relé activado sin tensión, `FALLO_VALVULA_MS`) detecta en realidad un fallo del relé o de la alimentación de las válvulas; un fallo de la válvula solo lo vería el caudalímetro (ver [Mejoras futuras](#mejoras-futuras-detección-de-fugas)).
- **RTC DS3231** (módulo ZS-042) — necesario porque el WiFi llega justo a las válvulas. Si se usa una CR2032 no recargable, quitar el diodo/resistencia del circuito de carga del módulo (pensado para pilas recargables).
- **LCD 16×2 con backpack I2C** en `0x27` (de fábrica, sin puentes). Usa la `LiquidCrystal_I2C` de LCDBigNumbers (ArminJo): incluir `<LiquidCrystal_I2C.hpp>`, no el `.h`, o falla al enlazar.
- **Dos PCF8574** (expansores I2C, mismo chip, direcciones distintas por A0-A2): uno en `0x20` (A0-A2 a GND, otra mochila de LCD) con **5 pulsadores sueltos montados en cruz** (arriba, abajo, izquierda, derecha, centro), común a GND; otro en `0x26` con las 4 entradas de estado de zona (salidas del módulo de optoacopladores): desde 2026-10-02 es un **módulo PCF8574 GERUI** con jumpers de dirección y P0-P7 directos, que sustituye a la mochila de LCD rota. Módulo opto Hailege probado 2026-09-26: salida en colector abierto sin pull-up propio, **activa a nivel bajo** (válvula con tensión → 0; 0,4 V con el pull-up del PCF8574, sin resistencia externa). El módulo no tiene serigrafía en la tira de pines y **va al revés**: mapeada el 2026-10-25 con `prueba_zonas/` (uniendo cada pin a GND), el 1.er pin no es de puerto (probablemente INT: suelto marcaba 1-2,3 V y se tomó por un P0 averiado) y siguen P7, P6, P5, P4. **Zona 1 = P4 (5.º pin), 2 = P7 (2.º), 3 = P6 (3.º), 4 = P5 (4.º)** (`BIT_ZONA` en `config.h`); P0-P3 libres. La GND de salida del módulo de optos tiene que ir a la GND del PCF (suelta, no se detecta ninguna zona). Sin transistor de retroiluminación, P3 ya no hay que evitarlo (con la mochila antigua era zonas 1-4 = P4, P2, P1, P0). La placa de botones no sigue el orden habitual de las mochilas; bits medidos en protoboard (2026-09-23): centro = P5, derecha = P4, arriba = P2, izquierda = P1, abajo = P0 (confirmado con los botones soldados, 2026-09-24). Al recolocar los botones (2026-10-03) cambió y se volvió a medir; **mapeo actual: centro = P2, derecha = P0, arriba = P4, abajo = P6, izquierda = P5**. Se valoraron y descartaron un joystick pequeño tipo Ardutype (esta caja va en la pared, no en la mano) y uno analógico KY-023 (necesitaría un ADS1115, más complejo y con deriva de centrado).
- **Electroválvulas: 24 V AC, normalmente cerradas (NC).** Al menos una es Marlux, 24 V AC, 2 W. Resistencia de las bobinas medida (2026-10-10): 43 Ω, 38 Ω y 10 Ω. La de 10 Ω no tiene marca y no es como las otras (¿12 V DC o biestable de 9 V?): se sustituye por una de 24 V AC. **Sin diodo 1N4007 en antiparalelo**: en alterna cortocircuitaría medio ciclo (como mucho, un varistor de ~30-40 V AC).

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

Contenido del menú (fase 3, 2026-10-03; `jarduino/menu.*`):

```
Menú
├─ Zonas
│  └─ Zona 1..4
│     ├─ Inicio        HH:MM
│     ├─ Fin           HH:MM
│     ├─ Días          L M X J V S D
│     ├─ Regar ahora   N min (por defecto 15, máx. 240)
│     └─ Parar
├─ Parar todo
├─ Fecha y hora      (por si no hay NTP)
└─ Sistema           IP, WiFi, RSSI, NTP, tiempo encendido, fecha de compilación
```

Al editar, el cursor parpadea sobre el campo; arriba/abajo cambian el valor (mantenidos, se repiten), derecha/izquierda pasan de campo (izquierda en el primero cancela) y centro corto guarda. Las acciones (Parar, Parar todo) solo se lanzan con el centro, no con derecha. Centro largo vuelve al inicio sin guardar, y tras 60 s sin tocar nada también. Nombres de zona, zona habilitada (`activo`), compañía y notificaciones solo se cambian desde la web.

## API HTTP (implementada en la fase 2, 2026-10-02)

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
3. ~~Montar en protoboard el bus I2C de la tapa y verificar direcciones~~ hecho 2026-09-23 (LCD `0x27` + botones `0x20`, sketches `i2c_scanner/` y `prueba_tapa/`). La tapa va entera a **5 V** (la LCD lo necesita): 4 hilos por la bisagra (5V, GND, SDA, SCL). Botones soldados y confirmados 2026-09-24. DS3231 y relés verificados 2026-09-24. Optoacopladores + PCF8574 de zonas (`0x26`) verificados 2026-09-27. En el montaje final (2026-09-30) la mochila de zonas dejó de responder en `0x26`. Causa (encontrada 2026-10-01): **soldadura defectuosa en el VCC de la mochila** (la continuidad salía según por qué lado se midiera). Sola en la protoboard funcionaba, seguramente alimentada a través de los pull-ups de SDA/SCL, pero en el circuito completo fallaba. Arreglada y confirmada con el escáner I²C. Además, la GND de salida del módulo de optos tiene que ir al pin 1 (VSS) del conector de 16 de la mochila; no basta la GND del conector del bus. El 2026-10-01 volvió a desaparecer: a esa mochila se le había partido un pin del conector de 4, al resoldarlo los pines dejaron de conectar con el chip (ni VCC ni SDA/SCL llegan) y hay un SMD con marca de quemado. **Se descarta** y se sustituye (2026-10-02) por un módulo PCF8574 GERUI con jumpers en `0x26`, recableado como zona n = P(n-1); probado en banco: optos, relés y detección de fallo. Antes de conectarla, revisar el cableado del módulo de optos (GND a GND, ninguna salida a 5 V o 19 V, sin corto VCC-GND). Mientras tanto se puede probar la fase 1 en el banco sin optos: el firmware solo avisa de que la placa de zonas no responde.
4. Firmware (`jarduino/`), por fases, con tareas no bloqueantes y el riego independiente de la red:
   1. **Núcleo sin red** — hecho 2026-09-27, compila sin avisos y las reglas de compañía se han probado en una simulación en el PC. Probado en el banco (2026-10-25): relés y detección de las 4 zonas por los optos; falta probar los interruptores manuales y la regla del jardín con las válvulas en su sitio. `config.json` en LittleFS, RTC, horarios (si arranca a mitad de una ventana, riega lo que queda), relés escalonados con la regla del jardín, optos (interruptor manual, fallo de válvula), pantalla de inicio de la tapa y consola serie (`T`, `R`, `P`, `E`, `C`; ver `consola.h`).
   2. **WiFi + NTP + API HTTP** — hecho y probado sobre hardware real por WiFi (2026-10-02). `red.*` (WiFiManager con portal cautivo "Jarduino-setup", mDNS `jarduino.local`, NTP que corrige el RTC al conectar y 1×/día) y `api.*` (ESP8266WebServer en el puerto 80, todos los endpoints del contrato, errores `{"error","detalle"}`). Comando serie `W` para ver el estado de la WiFi (SSID, IP, RSSI, NTP). El riego sigue funcionando sin red. Aviso: en el banco el RSSI era muy bajo (~-85 dBm); en el sitio final de las válvulas hará falta un extensor.
   3. **Menú de la tapa** — hecho y probado en la tapa (2026-10-03). `menu.*`, ver [Menú y LCD](#menú-y-lcd-5-pulsadores).
   4. OTA y web app.
   5. Notificaciones.

## Mejoras futuras: detección de fugas

Orden recomendado (charla del 2026-09-30):

1. **Caudalímetro** en la tubería general, después de los filtros y antes de la derivación a la abonadora. Antes de comprarlo, medir el caudal con el contador de la compañía (1, 2 y 3 zonas a la vez) para elegir el tamaño; seguramente de 1" (tipo YF-G1 / FS400A), porque el YF-S201 de 1/2" podría estrangular la tubería de 32 mm. Da pulsos, no es I²C: iría por interrupción en **RX/GPIO3** (D3, D4 y D8 son pines de arranque), que queda libre al pasar la consola a la red en la fase 2, con un divisor 10k/20k para bajar los pulsos a 3,3 V. Permite detectar:
   - **Fuga con todo cerrado**: caudal sostenido sin ninguna zona regando. Entre la llave general y las válvulas solo está la abonadora, así que es fiable; al cargarla o vaciarla puede verse una fuga pequeña, por eso esa alarma sería solo un aviso.
   - **Fuga durante el riego**: una zona gasta un 20-30 % más de lo que suele (tubo suelto, gotero roto). Si gasta menos, hay goteros atascados.
   - **Reventón**: el caudal se dispara → cerrar todo y avisar.
   - Límite: por debajo de ~1 L/min el rotor no gira, así que no ve goteos pequeños.
2. **Electroválvula general** al principio de la línea. Hoy "cerrar todo" solo cierra las 4 zonas: un reventón antes de las válvulas no se puede cortar. Se activaría desde la placa de zonas (`0x26`, P0-P3 libres) con un relé más. Además deja la línea sin presión cuando no se riega y permite la prueba de presión.
3. **Sensor de presión**: transductor de 0-1,2 MPa (0,5-4,5 V) con un ADS1115 (`0x48`) a 5 V en el bus I²C, después de los filtros y del reductor. Con la válvula general cerrada, si la presión baja hay una fuga, por pequeña que sea. También sirve para comprobar que una válvula abre, detectar cortes de agua y vigilar el reductor.

Si se añaden más placas al bus I²C, medir las pull-ups: con 4 placas el equivalente puede rondar ya 1,2 kΩ; quitar las que sobren.

## Agua (resumen)

Todas las preguntas de fontanería están respondidas (ver sección 9 de [`Jarduino_diseno_y_agua.md`](./Jarduino_diseno_y_agua.md#9-preguntas-para-mañana)): agua de red, tubería de 32 mm, riego por goteo, ubicación del caudalímetro identificada, y ante un reventón el sistema cierra todo y avisa. Falta solo medir la presión real y montar el caudalímetro — pospuesto.
