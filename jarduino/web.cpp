#include <Arduino.h>
#include <ESP8266WebServer.h>

#include "api.h"
#include "web.h"

// La página entera. Sin recursos externos: tiene que funcionar aunque la LAN
// no tenga salida a internet.
static const char PAGINA[] PROGMEM = R"web(<!doctype html>
<html lang="es">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="theme-color" content="#0b7f86">
<title>Jarduino</title>
<style>
:root{
  --bg:#f2f5f0; --surface:#ffffff; --ink:#1c2a23; --muted:#5b6a62; --line:#d7dfd8;
  --water:#0b7f86; --water-soft:#dff1f1; --leaf:#4c7a2c; --leaf-soft:#e7f0dc;
  --alert:#b83227; --alert-soft:#f8e1de; --wait:#a86a12; --wait-soft:#f6ead6;
  --sans:system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;
  --mono:ui-monospace,"SF Mono",Menlo,Consolas,monospace;
}
@media (prefers-color-scheme: dark){
  :root{
    --bg:#121815; --surface:#1b2420; --ink:#e6eee9; --muted:#97a89e; --line:#2c3832;
    --water:#3cb7bd; --water-soft:#163a3c; --leaf:#8cc163; --leaf-soft:#25331b;
    --alert:#ef7a6e; --alert-soft:#41201d; --wait:#e0a548; --wait-soft:#3a2c14;
    color-scheme:dark;
  }
}
*{box-sizing:border-box}
[hidden]{display:none!important}
body{margin:0;background:var(--bg);color:var(--ink);font:16px/1.45 var(--sans);
  padding:16px max(16px,env(safe-area-inset-right)) 48px max(16px,env(safe-area-inset-left))}
main{max-width:640px;margin:0 auto;display:flex;flex-direction:column;gap:16px}
h1{font-size:24px;margin:0;letter-spacing:-.01em}
h2{font-size:19px;margin:0}
button,select,input{font:inherit;color:inherit}
button{cursor:pointer;border-radius:10px;border:1px solid var(--line);background:var(--surface);
  padding:10px 16px;min-height:44px;font-weight:600}
button:focus-visible,select:focus-visible,input:focus-visible{outline:3px solid var(--water);outline-offset:2px}
button.primario{background:var(--water);border-color:var(--water);color:#fff}
button.peligro{background:var(--alert);border-color:var(--alert);color:#fff}
button:disabled{opacity:.5;cursor:default}
select,input[type=text],input[type=time]{border:1px solid var(--line);border-radius:10px;
  background:var(--surface);padding:9px 10px;min-height:44px}

.cabecera{display:flex;align-items:center;justify-content:space-between;gap:12px;flex-wrap:wrap}
.reloj{font:600 28px var(--mono);font-variant-numeric:tabular-nums}
.sub{color:var(--muted);font-size:14px}
.aviso{border-radius:12px;padding:12px 14px;background:var(--alert-soft);color:var(--alert);font-weight:600}
.aviso button{margin-top:8px}

.zona{background:var(--surface);border:1px solid var(--line);border-radius:14px;padding:16px;
  display:flex;flex-direction:column;gap:10px}
.zona header{display:flex;justify-content:space-between;align-items:center;gap:10px}
.pill{border-radius:999px;padding:4px 12px;font-size:14px;font-weight:700;white-space:nowrap}
.pill.cerrada{background:var(--bg);color:var(--muted)}
.pill.regando{background:var(--water-soft);color:var(--water)}
.pill.acompanando,.pill.mano{background:var(--leaf-soft);color:var(--leaf)}
.pill.espera{background:var(--wait-soft);color:var(--wait)}
.pill.alerta{background:var(--alert-soft);color:var(--alert)}
.detalle{margin:0}
.horario{margin:0;color:var(--muted);font-size:14px}
.horario b{color:var(--ink);font-family:var(--mono);font-weight:600}
.acciones{display:flex;gap:8px;flex-wrap:wrap}
.acciones .editar{margin-left:auto}

form.edicion{display:flex;flex-direction:column;gap:12px;border-top:1px solid var(--line);padding-top:12px}
form.edicion label{display:flex;flex-direction:column;gap:4px;font-size:14px;color:var(--muted)}
.fila{display:flex;gap:12px;flex-wrap:wrap}
.fila label{flex:1 1 120px}
.check{flex-direction:row!important;align-items:center;gap:10px!important;color:var(--ink)!important;font-size:16px!important}
.check input{width:22px;height:22px}
.dias{display:flex;gap:6px;flex-wrap:wrap}
.dias label{position:relative}
.dias input{position:absolute;opacity:0;inset:0}
.dias span{display:grid;place-items:center;width:42px;height:42px;border-radius:50%;
  border:1px solid var(--line);font-weight:700;color:var(--muted);cursor:pointer}
.dias input:checked+span{background:var(--water);border-color:var(--water);color:#fff}
.dias input:focus-visible+span{outline:3px solid var(--water);outline-offset:2px}

details{background:var(--surface);border:1px solid var(--line);border-radius:14px;padding:12px 16px}
summary{cursor:pointer;font-weight:600;min-height:28px}
dl{display:grid;grid-template-columns:max-content 1fr;gap:6px 16px;margin:12px 0 4px;font-size:15px}
dt{color:var(--muted)}
dd{margin:0;font-variant-numeric:tabular-nums;overflow-wrap:anywhere}
pre{font:13px/1.5 var(--mono);background:var(--bg);border-radius:10px;padding:10px 12px;margin:8px 0 12px;
  max-height:60vh;overflow:auto;white-space:pre-wrap;overflow-wrap:anywhere}

.toast{position:fixed;left:50%;bottom:max(16px,env(safe-area-inset-bottom));transform:translateX(-50%);
  background:var(--ink);color:var(--bg);padding:12px 18px;border-radius:12px;max-width:calc(100% - 32px);
  box-shadow:0 6px 24px rgba(0,0,0,.25)}
@media (prefers-reduced-motion:no-preference){.zona{transition:border-color .2s}}
</style>
</head>
<body>
<main>
  <div class="cabecera">
    <div><h1>Jarduino</h1><div class="sub" id="wifi">Conectando…</div></div>
    <div class="reloj" id="reloj">--:--</div>
  </div>

  <div class="aviso" id="sinConexion" hidden>No hay conexión con Jarduino. Reintentando…</div>
  <div class="aviso" id="avisoReloj" hidden>
    El reloj de Jarduino no tiene la hora buena, así que los horarios no se cumplirán.
    <br><button type="button" id="ponerHora">Poner la hora de este móvil</button>
  </div>
  <div class="aviso" id="avisoOptos" hidden>La placa que detecta las válvulas no responde: no se puede saber qué zonas tienen tensión.</div>

  <div id="zonas"></div>

  <button type="button" class="peligro" id="pararTodo">Parar todo</button>

  <details>
    <summary>Sistema</summary>
    <dl id="sistema"></dl>
  </details>

  <details id="registro">
    <summary>Registro</summary>
    <p class="sub">Lo último que ha hecho Jarduino. Cada línea empieza por el día del mes y la hora.</p>
    <pre id="log">Cargando…</pre>
    <div class="acciones">
      <button type="button" id="copiarLog">Copiar</button>
    </div>
  </details>

  <details id="consola">
    <summary>Consola</summary>
    <p class="sub">Las mismas órdenes que por el puerto serie: <b>E</b> estado, <b>W</b> WiFi, <b>C</b> configuración,
      <b>R 3 10</b> riega la zona 3 diez minutos, <b>P 3</b> o <b>P</b> detiene una zona o todas,
      <b>T2026-10-25 18:30:00</b> pone el reloj en hora.</p>
    <pre id="salidaConsola" hidden></pre>
    <form id="formConsola" class="fila">
      <input type="text" id="orden" maxlength="39" autocomplete="off" autocapitalize="characters"
             spellcheck="false" placeholder="Orden, p. ej. E" aria-label="Orden" style="flex:1 1 160px">
      <button type="submit" class="primario">Enviar</button>
    </form>
    <div class="acciones">
      <button type="button" class="rapida">E</button>
      <button type="button" class="rapida">W</button>
      <button type="button" class="rapida">C</button>
    </div>
  </details>
</main>
<div class="toast" id="toast" role="status" hidden></div>

<template id="plantillaZona">
  <article class="zona">
    <header><h2 class="nombre"></h2><span class="pill"></span></header>
    <p class="detalle"></p>
    <p class="horario"></p>
    <div class="acciones">
      <select class="minutos" aria-label="Minutos de riego">
        <option value="5">5 min</option><option value="10">10 min</option>
        <option value="15" selected>15 min</option><option value="20">20 min</option>
        <option value="30">30 min</option><option value="45">45 min</option>
        <option value="60">1 hora</option>
      </select>
      <button type="button" class="primario regar">Regar</button>
      <button type="button" class="detener">Detener</button>
      <button type="button" class="editar">Horario</button>
    </div>
    <form class="edicion" hidden>
      <label>Nombre<input type="text" name="nombre" maxlength="23" required></label>
      <label class="check"><input type="checkbox" name="activo">Riego automático</label>
      <div class="fila">
        <label>Empieza<input type="time" name="inicio" required></label>
        <label>Termina<input type="time" name="fin" required></label>
      </div>
      <div class="dias" role="group" aria-label="Días"></div>
      <div class="acciones">
        <button type="submit" class="primario">Guardar</button>
        <button type="button" class="cancelar">Cancelar</button>
      </div>
    </form>
  </article>
</template>

<script>
const DIAS = ['L','M','X','J','V','S','D'];
const NOMBRE_DIA = {L:'lunes',M:'martes',X:'miércoles',J:'jueves',V:'viernes',S:'sábado',D:'domingo'};
const ORIGEN = {automatico:'por horario', manual_api:'desde la web', manual_lcd:'desde la tapa', manual_switch:'con el interruptor'};
const $ = id => document.getElementById(id);
const tarjetas = {};
let zonas = [];

async function api(metodo, ruta, cuerpo) {
  const r = await fetch(ruta, {
    method: metodo,
    headers: cuerpo ? {'Content-Type': 'application/json'} : {},
    body: cuerpo ? JSON.stringify(cuerpo) : undefined
  });
  const datos = await r.json().catch(() => ({}));
  if (!r.ok) throw new Error(datos.detalle || ('Error ' + r.status));
  return datos;
}

let toastHasta = 0;
function avisar(texto) {
  const t = $('toast');
  t.textContent = texto;
  t.hidden = false;
  toastHasta = Date.now() + 3500;
  setTimeout(() => { if (Date.now() >= toastHasta) t.hidden = true; }, 3600);
}

function minutos(seg) {
  if (seg < 60) return 'menos de 1 min';
  const m = Math.ceil(seg / 60);
  return m >= 60 ? Math.floor(m / 60) + ' h ' + (m % 60) + ' min' : m + ' min';
}

function esc(t) {
  return String(t).replace(/[&<>"]/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
}

function nombreZona(id) {
  const z = zonas.find(z => z.id === id);
  return esc(z ? z.nombre : 'zona ' + id);
}

function estado(z) {
  const e = z.estado;
  if (e.fallo_valvula) return ['alerta', 'Sin tensión',
    'El relé está encendido pero no llega tensión a la válvula. Revisa el fusible y el transformador.'];
  if (e.pedida && e.rele) return ['regando', 'Regando',
    'Quedan ' + minutos(e.restante_seg) + ' · ' + (ORIGEN[e.activada_por] || '')];
  if (e.pedida) return ['espera', 'En espera',
    'Abrirá en unos segundos: las válvulas se mueven de una en una.'];
  if (e.por_compania) return ['acompanando', 'Acompañando',
    'Abierta para que otra zona no riegue sola.'];
  if (e.tension && !e.rele) return ['mano', 'Abierta a mano',
    'Se abrió con su interruptor. Ciérrala con el mismo interruptor.'];
  if (e.rele) return ['espera', 'Cerrando', 'Se cerrará en unos segundos.'];
  return ['cerrada', 'Cerrada', ''];
}

function textoHorario(z) {
  const h = z.horario;
  let t;
  if (!z.activo) t = 'Riego automático apagado';
  else if (h.fin <= h.inicio || !h.dias.length) t = 'Sin horario';
  else {
    const dias = h.dias.length === 7 ? 'todos los días' : h.dias.join(' ');
    t = 'Automático: <b>' + h.inicio + '–' + h.fin + '</b> · ' + dias;
  }
  if (z.necesita_compania.length)
    t += '<br>Nunca riega sola: va con ' + z.necesita_compania.map(nombreZona).join(' o ') + '.';
  return t;
}

function crearTarjeta(z) {
  const el = $('plantillaZona').content.firstElementChild.cloneNode(true);
  const n = z.id;
  const dias = el.querySelector('.dias');
  DIAS.forEach(d => {
    const l = document.createElement('label');
    l.innerHTML = '<input type="checkbox" name="dia" value="' + d + '" aria-label="' + NOMBRE_DIA[d] +
                  '"><span aria-hidden="true">' + d + '</span>';
    dias.appendChild(l);
  });
  el.querySelector('.regar').onclick = () => accion(el, () =>
    api('POST', '/api/riego/' + n + '/iniciar', {duracion_minutos: +el.querySelector('.minutos').value}));
  el.querySelector('.detener').onclick = () => accion(el, () => api('POST', '/api/riego/' + n + '/detener'));
  el.querySelector('.editar').onclick = () => abrirEdicion(el, n);
  el.querySelector('.cancelar').onclick = () => { el.querySelector('form').hidden = true; };
  el.querySelector('form').onsubmit = ev => { ev.preventDefault(); guardar(el, n); };
  $('zonas').appendChild(el);
  return el;
}

function pintarZona(z) {
  const el = tarjetas[z.id] || (tarjetas[z.id] = crearTarjeta(z));
  const [clase, texto, detalle] = estado(z);
  el.querySelector('.nombre').textContent = z.nombre;
  const pill = el.querySelector('.pill');
  pill.className = 'pill ' + clase;
  pill.textContent = texto;
  const d = el.querySelector('.detalle');
  d.textContent = detalle;
  d.hidden = !detalle;
  el.querySelector('.horario').innerHTML = textoHorario(z);
  el.querySelector('.regar').hidden = z.estado.pedida;
  el.querySelector('.minutos').hidden = z.estado.pedida;
  el.querySelector('.detener').hidden = !z.estado.pedida;
}

async function accion(el, llamada) {
  const botones = el.querySelectorAll('.acciones button');
  botones.forEach(b => b.disabled = true);
  try { const z = await llamada(); actualizarZona(z); }
  catch (e) { avisar(e.message); }
  finally { botones.forEach(b => b.disabled = false); }
}

function actualizarZona(z) {
  const i = zonas.findIndex(x => x.id === z.id);
  if (i >= 0) zonas[i] = z;
  pintarZona(z);
}

function abrirEdicion(el, n) {
  const z = zonas.find(z => z.id === n);
  const f = el.querySelector('form');
  f.nombre.value = z.nombre;
  f.activo.checked = z.activo;
  f.inicio.value = z.horario.inicio;
  f.fin.value = z.horario.fin;
  f.querySelectorAll('[name=dia]').forEach(c => c.checked = z.horario.dias.includes(c.value));
  f.hidden = false;
  f.nombre.focus();
}

async function guardar(el, n) {
  const f = el.querySelector('form');
  const nombre = f.nombre.value.trim();
  const dias = [...f.querySelectorAll('[name=dia]:checked')].map(c => c.value);
  if (!nombre || new TextEncoder().encode(nombre).length > 23)
    return avisar('El nombre tiene que tener entre 1 y 23 letras (las tildes cuentan doble).');
  if (f.activo.checked && f.fin.value <= f.inicio.value)
    return avisar('La hora de terminar tiene que ser posterior a la de empezar.');
  if (f.activo.checked && !dias.length)
    return avisar('Elige al menos un día, o apaga el riego automático.');
  try {
    const z = await api('PUT', '/api/status/' + n,
      {nombre, activo: f.activo.checked, horario: {inicio: f.inicio.value, fin: f.fin.value, dias}});
    actualizarZona(z);
    f.hidden = true;
    avisar('Horario guardado');
  } catch (e) { avisar(e.message); }
}

async function leerZonas() {
  try {
    const datos = await api('GET', '/api/status');
    zonas = datos.zonas;
    zonas.forEach(pintarZona);
    $('sinConexion').hidden = true;
  } catch (e) {
    $('sinConexion').hidden = false;
  }
}

function calidadWifi(rssi) {
  if (rssi >= -60) return 'buena';
  if (rssi >= -72) return 'aceptable';
  if (rssi >= -80) return 'débil';
  return 'muy débil';
}

function duracion(s) {
  const d = Math.floor(s / 86400), h = Math.floor(s % 86400 / 3600), m = Math.floor(s % 3600 / 60);
  return (d ? d + ' d ' : '') + (d || h ? h + ' h ' : '') + m + ' min';
}

async function leerSistema() {
  try {
    const s = await api('GET', '/api/sistema');
    const r = s.reloj;
    $('reloj').textContent = r.rtc_responde ? r.datetime.slice(11, 16) : '--:--';
    $('avisoReloj').hidden = r.reloj_valido;
    $('avisoOptos').hidden = s.optos_responden;
    const w = s.wifi;
    $('wifi').textContent = w.conectado ? 'WiFi ' + calidadWifi(w.rssi) : 'Sin WiFi';
    const filas = [
      ['Fecha y hora', r.rtc_responde ? r.datetime : 'el reloj no responde'],
      ['Hora por internet', r.ntp_sincronizado ? 'sincronizada' : 'todavía no'],
      ['WiFi', w.conectado ? w.ssid + ' · ' + w.rssi + ' dBm (' + calidadWifi(w.rssi) + ')' : 'desconectada'],
      ['Dirección', w.ip],
      ['Encendido hace', duracion(s.uptime_s)],
      ['Memoria libre', s.heap_libre + ' bytes'],
      ['Detección de válvulas', s.optos_responden ? 'funciona' : 'no responde'],
      ['Actualizar firmware', '/update']
    ];
    $('sistema').innerHTML = '';
    filas.forEach(([k, v]) => {
      const dt = document.createElement('dt'), dd = document.createElement('dd');
      dt.textContent = k;
      if (v === '/update') dd.innerHTML = '<a href="/update">/update</a> (pide clave)';
      else dd.textContent = v;
      $('sistema').append(dt, dd);
    });
  } catch (e) { /* lo avisa leerZonas */ }
}

$('ponerHora').onclick = async () => {
  const a = new Date(), p = x => String(x).padStart(2, '0');
  const datetime = a.getFullYear() + '-' + p(a.getMonth() + 1) + '-' + p(a.getDate()) + ' ' +
                   p(a.getHours()) + ':' + p(a.getMinutes()) + ':' + p(a.getSeconds());
  try { await api('PUT', '/api/datetime', {datetime}); avisar('Hora puesta'); leerSistema(); }
  catch (e) { avisar(e.message); }
};

let confirmarHasta = 0;
$('pararTodo').onclick = async () => {
  const b = $('pararTodo');
  if (Date.now() > confirmarHasta) {
    confirmarHasta = Date.now() + 4000;
    b.textContent = 'Pulsa otra vez para parar todo';
    setTimeout(() => { if (Date.now() > confirmarHasta) b.textContent = 'Parar todo'; }, 4100);
    return;
  }
  confirmarHasta = 0;
  b.textContent = 'Parar todo';
  try {
    const datos = await api('POST', '/api/riego/detener-todo');
    zonas = datos.zonas;
    zonas.forEach(pintarZona);
    avisar('Parando todas las zonas');
  } catch (e) { avisar(e.message); }
};

async function leerLog() {
  const pre = $('log');
  try {
    const r = await fetch('/api/log');
    if (!r.ok) throw new Error();
    const texto = (await r.text()).trimEnd() || 'Todavía no hay nada.';
    if (texto === pre.textContent) return;
    const alFinal = pre.scrollTop + pre.clientHeight >= pre.scrollHeight - 20;
    pre.textContent = texto;
    if (alFinal) pre.scrollTop = pre.scrollHeight;
  } catch (e) { /* lo avisa leerZonas */ }
}

$('registro').addEventListener('toggle', () => { if ($('registro').open) leerLog(); });
$('copiarLog').onclick = async () => {
  try { await navigator.clipboard.writeText($('log').textContent); avisar('Registro copiado'); }
  catch (e) { getSelection().selectAllChildren($('log')); avisar('Selecciona y copia el texto'); }
};

async function enviarOrden(orden) {
  orden = orden.trim();
  if (!orden) return;
  const pre = $('salidaConsola');
  let respuesta;
  try {
    const r = await fetch('/api/consola', {method: 'POST', headers: {'Content-Type': 'application/json'},
                                           body: JSON.stringify({orden})});
    respuesta = r.ok ? (await r.text()).trimEnd() : ((await r.json().catch(() => ({}))).detalle || 'Error ' + r.status);
  } catch (e) { respuesta = 'Sin conexión con Jarduino'; }
  if (!respuesta) respuesta = 'Hecho (lo que pase sale en el registro)';
  pre.textContent = (pre.textContent ? pre.textContent + '\n\n' : '') + '> ' + orden + '\n' + respuesta;
  pre.hidden = false;
  pre.scrollTop = pre.scrollHeight;
  leerZonas();
}

$('formConsola').onsubmit = ev => {
  ev.preventDefault();
  enviarOrden($('orden').value);
  $('orden').value = '';
};
document.querySelectorAll('.rapida').forEach(b => b.onclick = () => enviarOrden(b.textContent));

let ciclo = 0;
async function refrescar() {
  if (!document.hidden) {
    await leerZonas();
    if (ciclo % 5 === 0) await leerSistema();
    if (ciclo % 3 === 0 && $('registro').open) await leerLog();
    ciclo++;
  }
  setTimeout(refrescar, 2000);
}
document.addEventListener('visibilitychange', () => { if (!document.hidden) { ciclo = 0; } });
refrescar();
</script>
</body>
</html>
)web";

static void getPagina() {
  apiServidor().send_P(200, "text/html; charset=utf-8", PAGINA);
}

void webIniciar() {
  apiServidor().on("/", HTTP_GET, getPagina);
}
