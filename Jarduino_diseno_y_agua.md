# Jarduino: diseño hasta hoy y los problemas de agua

Documento para leer con calma, 18 de septiembre de 2026. Recoge lo decidido en los dos chats del 16 de septiembre y añade lo que planteas ahora sobre zonas, presión y caudal. Las cifras de precios y de rangos son orientativas y hay que verificarlas en las fichas antes de comprar.

## 1. Respuesta corta

- **Caudalímetros: sí existen y no son caros.** Los de turbina con sensor Hall, los típicos de Arduino, cuestan entre 3 y 15 €. Los caros son los electromagnéticos y los ultrasónicos industriales, y no hacen falta.
- **Antes de comprar el reductor de presión, conviene medir.** Un manómetro de 5-10 € en un grifo te da la presión estática y cómo cae con una, dos o tres zonas abiertas.
- **Con caudal y presión medidos, el firmware puede hacer más que regar:** detectar fugas, reventones y obstrucciones, y proteger al jardín para que nunca riegue solo.

## 2. Dónde estamos: el diseño actual

Es un resumen de lo que quedó cerrado. Sigue sin haber código.

**Alimentación y potencia (revisado 2026-09-21 sobre el prototipo real)**
- Fuente única: un cargador de portátil Dell reaprovechado, **19V / 3,1A** (barril), ya conectado al lado de conmutación de los relés en el prototipo.
- Conversor DC-DC **XL4005E1** (ajustable, hasta 5A/30V, ya en la caja de piezas del usuario), ajustado a 5,0V con el multímetro: alimenta el ESP8266, la bobina de los 4 relés y el resto de la lógica (LCD, RTC, los dos PCF8574).
- Módulo de 4 relés con el jumper VCC/JD-VCC **puesto** (decisión revisada: con 5A de margen en el XL4005 no hace falta separar bobina y lógica en dos fuentes, al contrario que el plan original con un LM2596).
- Los 19V, sin pasar por el buck, llegan también a los contactos de los relés y de ahí a cada electroválvula (dentro de su rango de 12-24V).
- **Retirado** del prototipo: el módulo pequeño de red (100-240VAC→5V/3W) que alimentaba el ESP por separado — se quita el único cable de tensión de red que había en la placa, más simple y más seguro.
- **Descartada** para este uso una fuente de 24V/5W (CX-4120) que el usuario tenía comprada a propósito: da como máximo ~208 mA, y una sola electroválvula ya pide 300-500 mA — insuficiente, y además reintroduciría cable de red en la placa. Se guarda para otro proyecto de bajo consumo.
- Un interruptor manual en paralelo con el contacto de cada relé, para regar a mano aunque el ESP falle.

**Estado de cada válvula**
- Un optoacoplador PC817 por zona, sobre la línea de 19 V hacia la válvula. Sirve tanto si abre el relé como si abre el interruptor manual.
- **Pedido 2026-09-21**: módulo Hailege de 4 canales PC817 ya montado (resistencia de entrada y salida incluidas, rango 3,6-30V — cubre los 19V sin problema), en vez de PC817 sueltos + resistencias. Un único módulo cubre las 4 zonas. Al llegar: comprobar en la ficha si la salida de cada canal es activa alta o baja, y si ya trae pull-up de salida. El usuario tiene además un buen kit de resistencias variado a mano, por si hiciera falta alguna suelta en otro punto.

**Lógica y pantalla**
- ESP8266 NodeMCU.
- Bus I2C compartido en D1 (SCL) y D2 (SDA) con la LCD 16x2 (misma placa que la caja, con su propio PCF8574T soldado — **0x27 de fábrica, confirmado 2026-09-23**), el DS3231 (0x68, confirmado en el prototipo real) y **dos PCF8574** más (2026-09-21, sustituyen al único expansor de antes): en total 3 chips PCF8574 en el mismo bus, direcciones 0x20 (botones) / 0x26 (zonas) / 0x27 (LCD).
- **PCF8574 botones, dirección 0x20** (A0-A2 a GND; se buscaba 0x21 pero el estaño unió también A0 y se dejó así, 2026-09-23): otra mochila de LCD con 5 pulsadores sueltos en cruz, común a GND (pin 1 del conector de 16). Orden de bits no estándar, confirmado con los botones soldados (2026-09-24): centro = P5, derecha = P4, arriba = P2, izquierda = P1, abajo = P0. P3 evitado (transistor de retroiluminación).
- **PCF8574 zonas, dirección 0x26** (solo A0 a GND, otra mochila de LCD; decidido 2026-09-24, un solo puente por ser menos soldadura): los 4 estados de zona, salidas del módulo Hailege de optoacopladores. Evitar P3 (transistor de retroiluminación: siempre a 0). Salidas del opto activas a nivel bajo, sin pull-up propio (usar el del PCF8574; nunca 5 V directos a V1). Zona 1 = pin 4 = P0 (2026-09-26); zonas 2-4 por mapear.
- **Alimentación de la tapa: todo a 5 V** (la LCD no muestra nada a 3,3 V y su mochila ya sube el bus a 5 V); 4 hilos por la bisagra: 5V, GND, SDA, SCL.
- Los dos son chips PCF8574 normales (no hace falta la variante "A"): con 3 pines de dirección hay 8 direcciones posibles (0x20-0x27), de sobra para dos módulos en el mismo bus.
- Relés: IN1→D7, IN2→D6, IN3→D5, IN4→D0 (medido con polímetro 2026-09-24, zona n = relé n; evita GPIO0/2/15 de arranque — el prototipo tenía IN1/IN2 en D3/D4, se corrigen por el mismo motivo que el HC-SR04 de la caja: un relé parpadeando en cada reset podría abrir una válvula sin querer).
- Quedan libres D3, D4, D8, RX y TX, y también A0.

**Menú con 5 pulsadores en cruz (2026-09-21, sustituye a los 3 botones; se valoró y descartó un joystick)**
- Se pensó primero en un joystick de 5 posiciones tipo Ardutype, pero esta caja va fija en la pared (no en la mano), así que el usuario prefiere 5 pulsadores sueltos, más fáciles de acertar. También se descartó un joystick analógico (KY-023): necesitaría un ADS1115 (2 canales, el ESP8266 solo tiene un pin analógico) y con el tiempo pierde el centrado — más complejidad y menos fiable sin ninguna ventaja aquí.
- Mismo convenio de gestos que Ardutype, aunque sean botones sueltos en vez de un joystick: arriba/abajo mueven el cursor o cambian valores.
- Derecha entra o avanza; izquierda vuelve atrás — mismo convenio que en Ardutype, para no reaprender gestos entre los dos cacharros.
- Pulsación central corta = confirma (sustituye a "OK corto"); pulsación central larga = entra al menú o vuelve al principio (sustituye a "OK largo").
- En la pantalla de inicio, pulsación central corta sobre una zona la activa o la detiene a mano.

**API y web**
- `GET` y `PUT` de `/api/datetime`.
- `GET /api/status` y `GET`/`PUT /api/status/{n}`, con horario (inicio, fin, días LMXJVSD) y un campo `activo`.
- `POST /api/riego/{n}/iniciar` (con duración), `POST /api/riego/{n}/detener` y `POST /api/riego/detener-todo`.
- `GET /api/sistema`.
- Campo `activada_por`: automático, interruptor manual o API.
- Una web app servida desde el propio ESP en `jarduino.local`, pensada para tu padre.
- NTP corrige al RTC, y las notificaciones (ntfy.sh o Telegram) llegan cuando la WiFi sea fiable.

**Descartado:** la LCD Keypad Shield, por pines, forma física y niveles de tensión.

## 3. Las zonas y la regla del jardín

Lo que cuentas:
1. **Huerto de frutales**, sobre todo naranjos.
2. **Huerta** (verdura).
3. **Jardín.**
4. **Sin asignar.** Quieres crearla para otros frutales.

Las zonas 1 y 2 pueden regarse solas o juntas. El jardín no puede regarse solo porque la presión es demasiada. Lo normal es regar las tres a la vez para evitar reventones y fugas.

Esto tiene una consecuencia clara para el diseño: **es una regla del sistema, no un detalle de horarios.** Hoy la API permite solapes libremente. Propongo añadirle a cada zona dos campos de configuración:

- `nombre`, editable. Así la zona 4 puede existir, con `activo: false`, hasta que la instales.
- `necesita_compania`, una lista de zonas de las que al menos una debe estar abierta. Para el jardín sería `[1, 2]`.

Reglas de firmware que salen de ahí:
- **Orden de apertura:** primero se abren las zonas acompañantes y después el jardín, con 3-5 segundos entre válvulas.
- **Orden de cierre:** primero se cierra el jardín y después las demás.
- **Los retardos escalonados** ayudan también con el golpe de ariete y con el pico de corriente al conectar varias válvulas a la vez.
- **El interruptor manual esquiva al firmware.** Si abres el jardín a mano sin ninguna otra zona, el relé no puede impedirlo. Pero los optoacopladores ven esa situación, y el ESP puede reaccionar abriendo la zona 1 y avisándote. Cerrar no puede, abrir sí.

**Resuelto (2026-09-21):** la zona 4 aún no existe físicamente — no hay por qué limitarla mientras no esté instalada. Cuando se monte, si hace falta, se le añade `necesita_compania` igual que al jardín; de momento se deja sin esa restricción y con `activo: false` en `config.json`.

## 4. Presión: primero medir, luego reducir

Un reductor es buena idea, pero hoy no sabes cuántos bares tienes. Todo el diseño depende de ese número.

**Resuelto (2026-09-21):** el agua viene de la red (no de pozo con bomba), con bastante presión, por tubería principal de 32 mm. Los bares concretos siguen sin medirse — eso es justo lo que pide el Paso 1, pospuesto por ahora.

**Paso 1: medir (5-10 €).** Un manómetro de 0-10 bar con adaptador de rosca para grifo. Anota tres valores:
- presión estática, con todo cerrado;
- presión con una zona abierta;
- presión con las tres zonas abiertas.

Esto también te dice cuánto cae la presión al regar todo junto, y por qué el jardín "funciona" acompañado y no solo.

**Paso 2: elegir el reductor.**
- Reductor de latón con manómetro, del diámetro de tu tubería (15-40 € aproximadamente).
- Lo habitual para goteo y aspersión doméstica está entre 1,5 y 3 bar. Confirma en los goteros o aspersores que tengas qué presión de trabajo piden.
- Si cada zona necesita una presión distinta (frutales frente a jardín), lo lógico son reductores por zona, y así desaparece la regla de compañía.
- Si todas piden lo mismo, un único reductor en la línea general, después del filtro y antes de las válvulas.

**Cuidado con las electroválvulas.** Muchas de las económicas, con membrana pilotada, necesitan una presión mínima (del orden de 0,3-0,5 bar) para abrir y cerrar bien, y tienen también un máximo. Si bajas mucho la presión, pueden no abrir del todo o no cerrar.

**Resuelto (2026-09-21):** las electroválvulas instaladas no tienen marca visible (llevan tiempo a la intemperie), funcionan entre 12 y 24 V — encajan sin problema con los 19V del cargador Dell reaprovechado — y son **normalmente cerradas (NC)**. Sin ficha, la presión mínima/máxima de apertura sigue sin confirmar; se verá en la práctica al probar con el reductor.

**Tipo de riego (resuelto 2026-09-21):** todo es goteo. Huerto y huerta llevan derivaciones de ~15 mm con goteros; el jardín también es goteo, pero con derivaciones de menor diámetro. Esto explica por qué hace falta reducir tanto la presión de red: los goteros suelen trabajar a 1-2 bar, muy por debajo de la presión "bastante fuerte" de una tubería de 32 mm. **Aviso de mantenimiento:** los goteros se ensucian con el sedimento del agua y hay que revisarlos de vez en cuando — no lo resuelve el firmware, pero conviene recordarlo (posible mejora futura: aviso periódico de mantenimiento).

**Sensor de presión electrónico (opcional, 8-15 €).** Los transductores de 0,5-4,5 V con rosca G1/4 y rango 0-12 bar son baratos. Necesitarían un divisor de tensión para entrar a A0, que está libre. Dejarían registrar la presión de verdad y detectar caídas.

## 5. Caudal: barato, con un par de trucos

**Qué comprar.**
- El clásico es el **YF-S201**: ½", unos 1-30 L/min, alrededor de 450 pulsos por litro. Es un sensor Hall dentro de una turbina de plástico y cuesta unos euros.
- Para más caudal existen versiones de ¾" y 1" (busca cosas como YF-B5, YF-G1 o "DN25 hall flow sensor"). Mira el rango en la ficha: con caudal muy fuerte, la turbina de ½" se queda pequeña.
- Precisión típica de ±2 a ±10 %. Sobra para detectar problemas y contar litros, pero no vale para facturación.
- La alternativa más robusta es un **contador de agua de pulsos** de 1" con emisor reed (20-40 € aproximadamente), pensado para agua de red.

**Cómo se conecta.**
- Salida de pulsos a un GPIO con interrupción. Sirven D3 o D4, o RX. Ojo: el sensor entrega 5 V, así que necesita divisor o transistor, como el HC-SR04 de la caja.
- D3 y D4 son pines de arranque. Es la misma lección que con la caja: que el sensor no fuerce un nivel raro durante el reset.
- D0 no admite interrupciones, y ya lo ocupa un relé.
- La frecuencia es baja (un par de cientos de hercios como mucho), sin problema para el ESP8266.

**Calibración casera (parte del cacharreo).** Llena un cubo con 10 litros medidos con precisión y cuenta los pulsos. Sale tu constante K real, mejor que la de la ficha.

**Montaje.**
- Tramo recto de tubería antes y después del sensor (mejor unas 10 veces el diámetro antes y 5 después).
- Un **filtro de malla o anillas** aguas arriba: si el agua trae sedimento, la turbina se atasca.
- Cuidado con la presión máxima del cuerpo del sensor. Muchos de plástico soportan alrededor de 17 bar, pero verifícalo.
- Cable corto y apantallado si es posible, y caja estanca.

### Alternativa casera: la abonadora como caudalímetro

Un caudalímetro de turbina no es más que un rotor que gira con el agua y un sensor que cuenta vueltas. Si por tu abonadora casera pasa toda el agua y tiene algo que gira, ya tienes el 80 % del aparato. Formas de contar las vueltas, de mejor a peor:

1. **Imán en el rotor más sensor Hall fuera de la carcasa** (A3144, US1881 o similar, 1-2 €). Si la pared es de plástico y no muy gruesa, el sensor lee el imán a través de ella sin tocar el agua: es a prueba de fugas, de suciedad y del abono. Un imán pequeño de neodimio pegado o embebido en una paleta basta.
2. **Interruptor reed en el exterior**, con el mismo imán. Aún más barato y sin electrónica, pero rebota y limita la velocidad, y es más frágil.
3. **Óptico** (TCRT5000 reflectivo o una horquilla óptica): necesita una ventana transparente limpia. Con agua, cal y abono se ensucia, así que lo dejaría como última opción.
4. **Destripar un YF-S201.** Por 3 € tienes una turbina ya calibrada de fábrica con su Hall dentro, y puedes copiar su forma para tu abonadora.

**Otra opción casera:** si tienes un contador de agua de esfera, muchos llevan un disco o una aguja que gira con el flujo. Se puede leer desde fuera con un sensor reflectivo o con un Hall, sin abrirlo. Es un hack muy conocido con ESP8266.

**Lo que hay que vigilar con una versión casera:**
- **El abono ensucia y cristaliza.** Un sensor que toque el agua con fertilizante disuelto se incrusta. El montaje magnético (opción 1) evita el problema.
- **Pérdida de carga.** Si el rotor estorba, con tu caudal fuerte puede restar presión. Pruébalo con el manómetro puesto.
- **Zona muerta.** A caudales muy bajos el rotor no gira, o gira a tirones. Cuenta poco cuando hay goteo mínimo, y sirve mejor con caudal medio o alto.
- **No es lineal de fábrica.** Vueltas por litro cambia con el caudal. Se calibra con el cubo de 10 litros y, mejor, con una tabla de dos o tres puntos.
- **Electrónica idéntica a la del comercial.** Un pulso por vuelta (o por imán) a un GPIO con interrupción, con la misma nota sobre pines de arranque y niveles de tensión.

**Actualización:** la abonadora es un depósito (barril de cerveza) con derivación. No tiene rotor y solo pasa por ella una fracción del caudal, que depende de la presión, así que no sirve para medir. El sensor iría en la línea general, antes de la derivación, o en un tramo de tubería con turbina (comercial o hecha en casa).

**Ubicación real del caudalímetro (resuelto 2026-09-21):** el tubo sale de una arqueta en el suelo con llave de cierre general — ahí se puede ubicar el caudalímetro. Ya hay dos filtros instalados (el agua trae bastante sedimento). Desde la arqueta, el tubo sube por una pared, gira a la izquierda (con un filtro en ese tramo), sigue recto, baja y gira formando un cuadrado; después vuelve a subir hasta las dos derivaciones hacia la abonadora, sube otra vez y gira a la derecha, donde cada medio metro hay una bajada con una electroválvula en medio. El caudalímetro puede ir en cualquier punto de ese recorrido.

## 6. Qué se puede hacer con el caudal medido

Un solo caudalímetro en la línea general basta para casi todo. Ideas ordenadas de más fácil y valiosa a más elaborada:

1. **Litros por riego y por zona.** Contar litros permite regar por volumen en lugar de por tiempo.
2. **Fuga con todo cerrado.** Si pasa caudal cuando las cuatro válvulas están cerradas, o hay una fuga o una válvula no cierra. Es la alerta más fácil y probablemente la más útil.
3. **"Huella" de caudal por combinación.** Cuando todo va bien, el sistema aprende el caudal normal de cada combinación (zona 1, zonas 1+2, 1+2+3, etc.). Después compara:
   - Mucho más de lo normal: posible reventón o fuga en el ramal.
   - Mucho menos: obstrucción, filtro sucio, válvula que no abrió o presión insuficiente.
4. **Acción ante un reventón (confirmado por el usuario, 2026-09-21).** Si el caudal se dispara, cerrar todo y avisar. Pero cerrar de golpe provoca golpe de ariete, así que el cierre debe ser escalonado, como se ha descrito antes.

Tolerancia: el caudal también depende de la presión, así que las comparaciones necesitan un margen amplio, del orden de ±25 %.

Todo esto casa con las notificaciones ya previstas: una fuga con todo cerrado merece un aviso inmediato por ntfy o Telegram.

## 7. Otros problemas que no son código

- **Golpe de ariete.** Es probablemente la causa de los reventones que mencionas: una válvula solenoide que cierra rápido genera un pico de presión. Los cierres escalonados ayudan, y a veces se añade un pequeño vaso de expansión.
- **Fuente de 12 V.** Cuatro válvulas a 0,3-0,5 A cada una suman 1,2-2 A, más el resto. Dimensiona con margen y usa el arranque escalonado. Cada solenoide necesita su diodo 1N4007 en antiparalelo, para absorber el pico al cortar la bobina. En lo que leí de los chats no aparece mencionado.
- **Humedad e intemperie.** Cajas estancas IP65, prensaestopas, y protección para la electrónica frente a condensación. Un NodeMCU en una caja cerrada al sol se calienta.
- **Pila del DS3231.** El módulo ZS-042 tiene un circuito de carga pensado para pilas recargables. Con una CR2032 normal hay que quitar el diodo o la resistencia del circuito de carga.
- **LED de estado por zona.** El README original pedía uno por botón. Puedes colgarlo de la línea de 12 V con una resistencia (unos 1 kΩ), sin usar ningún GPIO.

## 8. Lista de la compra tentativa

Precios orientativos, a verificar.

1. Manómetro 0-10 bar con adaptador a grifo (5-10 €). **Esto primero.**
2. Caudalímetro Hall según diámetro (3-15 €), o contador de pulsos de 1" (20-40 €). Si la abonadora sirve, basta un sensor Hall suelto (1-2 €) y un imán de neodimio pequeño.
3. Filtro de malla o anillas, si no tienes ninguno antes de las válvulas.
4. Reductor de presión con manómetro, del diámetro de la tubería (15-40 €). **Solo después de medir.**
5. Transductor de presión 0,5-4,5 V, G1/4 (8-15 €), opcional.
6. Diodos 1N4007, resistencias y transistor o divisor para el caudalímetro.
7. Cajas IP65, prensaestopas, cable apantallado.
8. Racores, cinta de teflón y adaptadores de rosca.

## 9. Preguntas para mañana

Todas resueltas el 2026-09-21 (la parte física — medir presión, instalar el caudalímetro — queda pospuesta; esto es solo la información):

1. **Resuelta.** Agua de red (no pozo/bomba), con bastante presión. Tubería principal de 32 mm. (Bares por medir — sección 4, Paso 1, pospuesto.)
2. **Resuelta.** Sin marca visible (intemperie), funcionan entre 12 y 24 V, normalmente cerradas (NC). Ver sección 4.
3. **Resuelta.** Todo es goteo. Huerto y huerta con derivaciones de ~15 mm; jardín también goteo, con derivaciones de menor diámetro. Ver sección 4.
4. **Resuelta.** La zona 4 aún no existe — no hace falta limitarla por ahora. Ver sección 3.
5. **Resuelta.** Sí hay tramo recto y accesible, y ya hay dos filtros instalados (agua con bastante sedimento). Aviso de mantenimiento: hay que revisar y limpiar los goteros de vez en cuando. Ver sección 5.
6. **Resuelta.** Ahora mismo se riega por tiempo, no se conocen los litros — coincide con cómo ya está pensada la API (`duracion_minutos`).
7. **Resuelta.** Ante un reventón: cerrar todo y avisar. Ver sección 6.
8. **Resuelta.** El tubo sale de una arqueta en el suelo con llave de cierre general (ahí puede ir el caudalímetro), sube por una pared, gira a la izquierda (con un filtro), sigue recto, baja y gira formando un cuadrado; luego sube hasta las dos derivaciones hacia la abonadora, vuelve a subir y gira a la derecha, donde cada medio metro hay una bajada con una electroválvula. El caudalímetro puede ir en cualquier punto de ese recorrido. Ver sección 5.
9. **Resuelta.** README reescrito el 2026-09-21.

## 10. Siguientes pasos que propongo

1. ~~Medir presiones con el manómetro.~~ **Pospuesto** — el usuario deja la fontanería para más adelante (2026-09-21).
2. ~~Reescribir el README con el diseño real, incluida la regla de zonas.~~ **Hecho** (2026-09-21).
3. ~~Definir el formato de `config.json`~~ **Hecho** (2026-09-21) — ver sección 11.
4. ~~Montar en protoboard el bus I2C de la tapa y verificar direcciones~~ hecho 2026-09-23 (LCD 0x27, botones 0x20). DS3231 y relés verificados 2026-09-24; falta PCF8574 de zonas (0x26) con los optos.
5. Después, el firmware, con estructura de tareas no bloqueantes y el riego independiente de la red.


## 11. Formato de `config.json` (2026-09-21)

Persistencia en LittleFS, vía ArduinoJson v7 (igual que la caja). Solo guarda **configuración**, no el estado de riego en curso (eso vive en RAM y lo expone `GET /api/status` en caliente). Ejemplo completo en [`config.json.example`](./config.json.example).

```json
{
  "version": 1,
  "zonas": [
    {
      "id": 1,
      "nombre": "Huerto",
      "activo": true,
      "necesita_compania": [],
      "horario": { "inicio": "07:00", "fin": "07:20", "dias": ["L", "M", "X", "J", "V", "S", "D"] }
    },
    {
      "id": 2,
      "nombre": "Huerta",
      "activo": true,
      "necesita_compania": [],
      "horario": { "inicio": "07:20", "fin": "07:40", "dias": ["L", "M", "X", "J", "V", "S", "D"] }
    },
    {
      "id": 3,
      "nombre": "Jardín",
      "activo": true,
      "necesita_compania": [1, 2],
      "horario": { "inicio": "07:00", "fin": "07:40", "dias": ["L", "X", "V"] }
    },
    {
      "id": 4,
      "nombre": "Sin asignar",
      "activo": false,
      "necesita_compania": [],
      "horario": { "inicio": "00:00", "fin": "00:00", "dias": [] }
    }
  ],
  "sistema": {
    "notificaciones": { "activo": false, "servicio": "ninguno", "destino": "" }
  }
}
```

**Campos por zona:**
- `id`: fijo, 1-4, coincide con el pin de relé y con `{n}` en `/api/status/{n}` y `/api/riego/{n}/...`.
- `nombre`: editable desde la web app.
- `activo`: automático encendido/apagado para esa zona. También sirve para "aún no instalada" (zona 4 hoy) — mismo campo, sin distinguir los dos casos, tal como se decidió en la sección 3.
- `necesita_compania`: lista de `id` de zonas de las que al menos una debe estar abierta para que esta se abra (la del jardín es `[1, 2]`); vacía si no aplica. El firmware la usa para el orden de apertura/cierre escalonado (sección 3).
- `horario.inicio` / `horario.fin`: `"HH:MM"`, hora local (la del RTC). La duración del riego automático sale de la resta; el riego manual (`POST /api/riego/{n}/iniciar`) manda su propia `duracion_minutos` y no toca este horario.
- `horario.dias`: iniciales en español, el mismo convenio `L M X J V S D` ya usado en el diseño (lunes a domingo); vacío significa que esa zona nunca riega en automático, solo a mano.

**`sistema.notificaciones`:** `servicio` será `ninguno` | `ntfy` | `telegram`; `destino` es el topic de ntfy o el chat_id de Telegram — no es secreto por sí solo. El **token del bot de Telegram** o la **URL de acceso de ntfy**, si hace falta, van en un `secrets.json` aparte, subido a LittleFS a mano y con `.gitignore` — igual que `write-pass/config.h` en Ardutype, nunca en `config.json` ni en git.

Pendiente para cuando se aborde la fontanería: cuando haya caudalímetro instalado, un bloque nuevo (por ejemplo `sistema.caudal`) con los umbrales de reventón/fuga (sección 6) — no hace falta diseñarlo ahora, `version: 1` deja hueco para una migración.
