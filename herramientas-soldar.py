# Lee el JSON exportado de stripboard-editor.com y calcula, para placa
# perforada, qué caminos de estaño y qué puentes hay que hacer.
import io, json, sys

d = json.load(io.open(sys.argv[1], encoding='utf-8'))
B = d['board']
FILAS, COLS = B['rows'], B['cols']
nets = {n['id']: n for n in d['nets']}
NOMBRE = {'net1': 'MOSI', 'net2': 'RESET', 'net3': 'DC', 'net4': 'MISO', 'net5': 'SCK', 'net6': 'CS'}
for n in nets.values():
    n['name'] = NOMBRE.get(n['name'], n['name'])
comp = {c['id']: c for c in d['components']}

ESP_NOMBRES = ['3V3', 'EN', 'IO36', 'IO39', 'IO34', 'IO35', 'IO32', 'IO33', 'IO25', 'IO26', 'IO27', 'IO14', 'IO12', 'GND', 'IO13', 'IO9', 'IO10', 'IO11', 'VIN',
               'IO6', 'IO7', 'IO8', 'IO15', 'IO2', 'IO0', 'IO4', 'IO16', 'IO17', 'IO5', 'IO18', 'IO19', 'GND', 'IO21', 'IO3', 'IO1', 'IO22', 'IO23', 'GND']


def pin_pos(c, pin):
    r0, c0 = c['boardPos']['row'], c['boardPos']['col']
    p = int(pin)
    if c['defId'].startswith('def-esp32'):
        return (r0 + p - 1, c0) if p <= 19 else (r0 + 38 - p, c0 + 10)
    n = int(c['defId'].split('-')[-1])
    if c.get('rotation', 0) == 180:
        return (r0 + n - p, c0)
    return (r0 + p - 1, c0)


pads = {}   # (fila, col) -> (componente, pin)
for c in d['components']:
    n = 38 if c['defId'].startswith('def-esp32') else int(c['defId'].split('-')[-1])
    for p in range(1, n + 1):
        pads[pin_pos(c, p)] = (c['label'], p)
red_pad = {}
for a in d['netAssignments']:
    red_pad[pin_pos(comp[a['componentId']], a['pinId'])] = nets[a['netId']]['name']

cortes = {(x['row'], x['col']) for x in B['cuts']}
puentes = [((w['from']['row'], w['from']['col']), (w['to']['row'], w['to']['col'])) for w in B['wires']]

# tramos de cada fila (entre cortes) con los puntos usados
tramos = []
for f in range(FILAS):
    ini = 0
    for c in range(COLS + 1):
        if c == COLS or (f, c) in cortes:
            usados = [x for x in range(ini, c) if (f, x) in red_pad or any((f, x) in w for w in puentes)]
            if len(usados) >= 2:
                red = next((red_pad[(f, x)] for x in usados if (f, x) in red_pad), None)
                tramos.append({'fila': f, 'desde': min(usados), 'hasta': max(usados), 'red': red})
            ini = c + 1
# red de los tramos que solo tienen puentes
for _ in range(5):
    for t in tramos:
        if t['red'] is None:
            for a, b in puentes:
                for p, q in ((a, b), (b, a)):
                    if p[0] == t['fila'] and t['desde'] <= p[1] <= t['hasta']:
                        for u in tramos:
                            if u['red'] and u['fila'] == q[0] and u['desde'] <= q[1] <= u['hasta']:
                                t['red'] = u['red']
ocupado = {}
for t in tramos:
    for x in range(t['desde'], t['hasta'] + 1):
        ocupado[(t['fila'], x)] = t['red']
for (f, x), (lab, p) in pads.items():
    ocupado.setdefault((f, x), 'pad')

# un puente vertical se puede hacer con estaño si no pisa nada en el medio
res_puentes = []
for a, b in puentes:
    (f1, c1), (f2, c2) = sorted([a, b])
    red = ocupado.get(a) if ocupado.get(a) not in (None, 'pad') else ocupado.get(b)
    libre = c1 == c2 and all((f, c1) not in ocupado for f in range(f1 + 1, f2))
    res_puentes.append({'a': (f1, c1), 'b': (f2, c2), 'red': red, 'estano': libre})


# ---------- hoja para imprimir ----------
def L(f, c):
    return '%s%d' % (chr(65 + f), c + 1)


COLOR = {n['name']: n['color'] for n in nets.values()}
COLOR['GND'] = '#333'
P = 5.6  # mm entre agujeros en el dibujo


def dibujo(espejo):
    W, H = COLS * P + 14, FILAS * P + 22
    o = ['<svg viewBox="0 0 %.1f %.1f" width="%.1fmm" height="%.1fmm">' % (W, H, W, H)]
    X = lambda c: 10 + ((COLS - 1 - c) if espejo else c) * P + P / 2
    Y = lambda f: 10 + f * P + P / 2
    o.append('<rect x="8" y="8" width="%.1f" height="%.1f" rx="2" fill="%s" stroke="#8a6d3b" stroke-width="0.4"/>' % (COLS * P + 4, FILAS * P + 4, '#efd2a6' if espejo else '#e4c79c'))
    for c in range(COLS):
        o.append('<text x="%.1f" y="6.5" class="n">%d</text>' % (X(c), c + 1))
    for f in range(FILAS):
        o.append('<text x="4" y="%.1f" class="n">%s</text>' % (Y(f) + 1, chr(65 + f)))
    if not espejo:
        cx = (X(6) + X(16)) / 2
        o.append('<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" rx="1.5" fill="#2e7d5b" fill-opacity="0.15" stroke="#2e7d5b" stroke-width="0.5" stroke-dasharray="1.5 1"/>' % (X(6) - 3.5, Y(0) - 4, X(16) - X(6) + 7, Y(18) - Y(0) + 11))
        o.append('<rect x="%.1f" y="%.1f" width="9" height="5" fill="#999" stroke="#555" stroke-width="0.3"/>' % (cx - 4.5, Y(18) + 7))
        o.append('<text x="%.1f" y="%.1f" class="g">ESP32</text>' % (cx, Y(8)))
        o.append('<text x="%.1f" y="%.1f" class="m">(enchufada en 2 tiras hembra)</text>' % (cx, Y(9) + 1))
        o.append('<text x="%.1f" y="%.1f" class="m">USB</text>' % (cx, Y(18) + 15))
    def linea(f1, c1, f2, c2, col):
        o.append('<line x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f" stroke="%s" stroke-width="%s" stroke-linecap="round" %s/>' % (
            X(c1), Y(f1), X(c2), Y(f2), col, 2.2 if espejo else 1.0, '' if espejo else 'stroke-opacity="0.3"'))
    for t in tramos:
        linea(t['fila'], t['desde'], t['fila'], t['hasta'], COLOR.get(t['red'], '#777'))
    for p in res_puentes:
        col = COLOR.get(p['red'], '#777')
        (f1, c1), (f2, c2) = p['a'], p['b']
        if p['estano']:
            linea(f1, c1, f2, c2, col)
        elif espejo:
            for f, c in p['a'], p['b']:
                o.append('<circle cx="%.1f" cy="%.1f" r="2" fill="none" stroke="%s" stroke-width="0.6" stroke-dasharray="0.8 0.6"/>' % (X(c), Y(f), col))
        else:
            o.append('<line x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f" stroke="%s" stroke-width="1.3" stroke-linecap="round"/>' % (X(c1), Y(f1), X(c2), Y(f2), col))
            for f, c in p['a'], p['b']:
                o.append('<circle cx="%.1f" cy="%.1f" r="1.1" fill="%s"/>' % (X(c), Y(f), col))
    for f in range(FILAS):
        for c in range(COLS):
            if (f, c) in pads:
                lab, pin = pads[(f, c)]
                o.append('<rect x="%.1f" y="%.1f" width="3.4" height="3.4" fill="%s" stroke="#5a4210" stroke-width="0.3"/>' % (X(c) - 1.7, Y(f) - 1.7, '#bbb' if lab == 'U1' else '#f2c94c'))
            else:
                o.append('<circle cx="%.1f" cy="%.1f" r="0.6" fill="#8a6d3b"/>' % (X(c), Y(f)))
    for (f, c), (lab, pin) in pads.items():
        if lab == 'U1' and (not espejo or (f, c) in red_pad):
            afuera = (c == 6) != espejo
            o.append('<text x="%.1f" y="%.1f" class="p" text-anchor="%s">%s</text>' % (
                X(c) + (-2.4 if afuera else 2.4), Y(f) + 0.8, 'end' if afuera else 'start', ESP_NOMBRES[pin - 1]))
    o.append('</svg>')
    return '\n'.join(o)


CONECTORES = [('J1', 'Pantalla', ['GND', 'MOSI', 'RESET', 'DC', 'MISO', 'SCK', 'CS', 'VCC', 'LED']),
              ('J2', 'Botones', ['GND', 'NAV', 'SEL', 'ATRÁS']),
              ('J3', 'Buzzer', ['S', '+', '−']),
              ('J4', 'Batería (cables del step-up)', ['GND', '5V'])]


def item(col, texto, red):
    return '<li><span class="dot" style="background:%s"></span>%s <span class="red">%s</span></li>' % (col, texto, red)


def html():
    piezas = []
    for lab, que, noms in CONECTORES:
        c = next(x for x in d['components'] if x['label'] == lab)
        pos = sorted((pin_pos(c, i + 1), n) for i, n in enumerate(noms))
        piezas.append('<tr><td><b>%s</b> %s</td><td>%s</td></tr>' % (lab, que, ', '.join('%s <b>%s</b>' % (n, L(*p)) for p, n in pos)))
    estano = [item(COLOR.get(t['red'], '#777'), '<b>%s</b> → <b>%s</b>' % (L(t['fila'], t['desde']), L(t['fila'], t['hasta'])), t['red'])
              for t in sorted(tramos, key=lambda t: (t['red'] or '', t['fila']))]
    estano += [item(COLOR.get(p['red'], '#777'), '<b>%s</b> ↓ <b>%s</b>' % (L(*p['a']), L(*p['b'])), p['red'] + ' (vertical)')
               for p in res_puentes if p['estano']]
    puente = [item(COLOR.get(p['red'], '#777'), '<b>%s</b> ↔ <b>%s</b>' % (L(*p['a']), L(*p['b'])), p['red'])
              for p in res_puentes if not p['estano']]
    return PLANTILLA % (dibujo(False), '\n'.join(piezas), len(puente), '\n'.join(puente),
                        dibujo(True), len(estano), '\n'.join(estano))


PLANTILLA = '''<!doctype html><html lang="es"><head><meta charset="utf-8"><title>Chaca - placa para soldar</title>
<style>
@page { size: A4 landscape; margin: 0 }
body { margin: 0; font-family: Segoe UI, Arial, sans-serif; color: #222; -webkit-print-color-adjust: exact; print-color-adjust: exact }
.hoja { width: 297mm; height: 210mm; box-sizing: border-box; padding: 8mm 10mm; page-break-after: always; display: flex; gap: 6mm; overflow: hidden }
.hoja:last-child { page-break-after: auto }
h1 { font-size: 14pt; margin: 0 0 1mm } h2 { font-size: 10.5pt; margin: 3mm 0 1mm }
.sub { font-size: 8.5pt; color: #555; margin: 0 0 1.5mm }
.col { flex: 1; font-size: 8.5pt; line-height: 1.35 }
ol { margin: 0; padding-left: 6mm } li { margin: 0.5mm 0 }
.dot { display: inline-block; width: 3mm; height: 3mm; border-radius: 50%%; vertical-align: -0.4mm; margin-right: 1.5mm }
.red { color: #777 } table { border-collapse: collapse; font-size: 8.5pt } td { padding: 0.7mm 1.5mm; border-bottom: 0.2mm solid #ddd; vertical-align: top }
.ojo { background: #fff4d6; border-left: 1mm solid #e0a800; padding: 1.5mm 2.5mm; margin: 2mm 0; font-size: 8.5pt }
.lista { columns: 2; column-gap: 6mm }
svg text.n { font-size: 2.6px; text-anchor: middle; fill: #555 } svg text.p { font-size: 2.1px; fill: #222 }
svg text.g { font-size: 4px; font-weight: 700; text-anchor: middle; fill: #2e7d5b } svg text.m { font-size: 2.4px; text-anchor: middle; fill: #2e7d5b }
</style></head><body>

<div class="hoja"><div>
<h1>1. Lado de los componentes (arriba)</h1>
<p class="sub">Dónde va cada pieza y los puentes. Los caminos de estaño se ven clarito porque están del otro lado.</p>
%s</div>
<div class="col">
<div class="ojo"><b>Antes de soldar nada:</b><br>
• Columna <b>1</b> = la primera de la <b>izquierda</b> mirando de este lado, con la fila <b>A</b> arriba. Si los números impresos en tu placa van al revés, guiate por esta hoja y numerá vos con fibra.<br>
• Contá la distancia entre las 2 filas de pines de tu ESP32: tiene que coincidir con columna <b>7</b> y columna <b>17</b>. Si no coincide, NO sueldes: avisame.</div>
<h2>Piezas</h2>
<table>
<tr><td><b>U1</b> ESP32</td><td>2 tiras HEMBRA de 19: <b>A7 a S7</b> y <b>A17 a S17</b>. El 3V3 queda en A7, VIN (5V) en S7 y el USB para abajo.</td></tr>
%s
</table>
<h2>Puentes (%d): cablecito con aislante, por arriba</h2>
<p class="sub">Pelá las puntas, pasalas por los 2 agujeros y soldalas del lado del cobre, pegadas al camino que llega ahí.</p>
<ol>%s</ol>
</div></div>

<div class="hoja"><div>
<h1>2. Lado del cobre (abajo), espejado</h1>
<p class="sub">Así se ve cuando das vuelta la placa: la columna 1 queda a la <b>derecha</b>. Línea de color = camino de estaño. Círculo punteado = ahí llega un puente.</p>
%s</div>
<div class="col">
<h2>Caminos de estaño (%d)</h2>
<p class="sub">Uní agujero por agujero, solo entre los dos que dice. Mismo color = misma conexión.</p>
<ol class="lista">%s</ol>
<div class="ojo"><b>Al terminar, con el tester en modo pitido (sin la ESP32 puesta):</b><br>
• Entre <b>GND</b> y <b>3V3</b> NO tiene que pitar. Entre <b>GND</b> y <b>5V</b> tampoco.<br>
• Cada pin de J1 tiene que pitar con su pin de la ESP32 (por ejemplo, CS de J1 con IO5).<br>
• Mirá a contraluz que ninguna gota de estaño toque un camino vecino.</div>
</div></div>
</body></html>'''

if __name__ == '__main__':
    if len(sys.argv) > 2:
        io.open(sys.argv[2], 'w', encoding='utf-8').write(html())
        print('hoja ok')
    else:
        for t in tramos:
            print('estaño', t['red'], L(t['fila'], t['desde']), '->', L(t['fila'], t['hasta']))
        for p in res_puentes:
            print('ESTAÑO' if p['estano'] else 'PUENTE', p['red'], L(*p['a']), '->', L(*p['b']))
