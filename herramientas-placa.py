import io, sys
p = 5.08          # 2.54 mm x 2 (dibujo a escala 2:1)
OX, OY = 14, 34   # esquina del dibujo
COLS, FILAS = 27, 19
arriba = ['GND', 'G23', 'G22', 'TX', 'RX', 'G21', 'GND', 'G19', 'G18', 'G5', 'G17', 'G16', 'G4', 'G0', 'G2', 'G15', 'SD1', 'SD0', 'CLK']
abajo = ['3V3', 'EN', 'SVP', 'SVN', 'G34', 'G35', 'G32', 'G33', 'G25', 'G26', 'G27', 'G14', 'G12', 'GND', 'G13', 'SD2', 'SD3', 'CMD', '5V']
X0 = 7
YA, YB = 4, 14
o = []


def h0(x, y):
    return (OX + (x + 0.5) * p, OY + (y + 0.5) * p)


def linea(pts, color, ancho=1.0, dash=''):
    d = ' '.join(('M' if i == 0 else 'L') + '%.2f %.2f' % h0(*q) for i, q in enumerate(pts))
    extra = ' stroke-dasharray="%s"' % dash if dash else ''
    o.append('<path d="%s" stroke="%s" stroke-width="%s" fill="none" stroke-linecap="round" stroke-linejoin="round"%s/>' % (d, color, ancho, extra))


o.append('<rect x="%s" y="%s" width="%.2f" height="%.2f" rx="2" fill="#e9d8b4" stroke="#8a6d3b" stroke-width="0.5"/>' % (OX, OY, COLS * p + p, FILAS * p + p))
for x in range(COLS):
    for y in range(FILAS):
        cx, cy = h0(x, y)
        o.append('<circle cx="%.2f" cy="%.2f" r="0.55" fill="#b08d57"/>' % (cx, cy))

# rieles
linea([(0, 1), (26, 1)], '#111', 1.6)
linea([(0, 18), (26, 18)], '#d00', 1.6)

# ESP32 y tiras hembra
ex0, ey0 = h0(X0 - 0.9, YA - 1.3)
ex1, ey1 = h0(X0 + 18 + 2.6, YB + 1.3)
o.append('<rect x="%.2f" y="%.2f" width="%.2f" height="%.2f" rx="2" fill="#333" fill-opacity="0.12" stroke="#333" stroke-width="0.4" stroke-dasharray="2 1"/>' % (ex0, ey0, ex1 - ex0, ey1 - ey0))
for yy in (YA, YB):
    a0 = h0(X0 - 0.45, yy - 0.45)
    a1 = h0(X0 + 18 + 0.45, yy + 0.45)
    o.append('<rect x="%.2f" y="%.2f" width="%.2f" height="%.2f" fill="#222"/>' % (a0[0], a0[1], a1[0] - a0[0], a1[1] - a0[1]))
for fila, nombres in ((YA, arriba), (YB, abajo)):
    for i, n in enumerate(nombres):
        cx, cy = h0(X0 + i, fila)
        o.append('<circle cx="%.2f" cy="%.2f" r="0.9" fill="#ddd"/>' % (cx, cy))
        if fila == YA:
            ty = cy + 2.6
            o.append('<text x="%.2f" y="%.2f" class="pin" transform="rotate(70 %.2f %.2f)">%s</text>' % (cx - 0.6, ty, cx - 0.6, ty, n))
        else:
            ty = cy - 2.6
            o.append('<text x="%.2f" y="%.2f" class="pin" transform="rotate(-70 %.2f %.2f)">%s</text>' % (cx - 0.6, ty, cx - 0.6, ty, n))
ux, uy = h0(X0 + 18 + 2.0, (YA + YB) / 2)
o.append('<rect x="%.2f" y="%.2f" width="6" height="8" fill="#999" stroke="#111" stroke-width="0.3"/>' % (ux - 1, uy - 4))
o.append('<text x="%.2f" y="%.2f" class="pin">USB-C</text>' % (ux + 6, uy + 1))
mx, my = h0(X0 + 9, 9)
o.append('<text x="%.2f" y="%.2f" class="lbl" text-anchor="middle">ESP32 (enchufada en 2 tiras de pines HEMBRA)</text>' % (mx, my + 1))


def conector(pts, nombres, lado):
    for (x, y), n in zip(pts, nombres):
        cx, cy = h0(x, y)
        o.append('<rect x="%.2f" y="%.2f" width="3" height="3" fill="#f2c94c" stroke="#7a5b00" stroke-width="0.3"/>' % (cx - 1.5, cy - 1.5))
        if lado == 'izq':
            o.append('<text x="%.2f" y="%.2f" class="pin" text-anchor="end">%s</text>' % (cx - 2.6, cy + 0.9, n))
        else:
            o.append('<text x="%.2f" y="%.2f" class="pin" text-anchor="middle">%s</text>' % (cx, cy + 4.3, n))


pant = [(4, 4 + i) for i in range(9)]
conector(pant, ['VCC', 'GND', 'CS', 'RESET', 'DC', 'MOSI', 'SCK', 'LED', 'MISO'], 'izq')
cx, cy = h0(4, 4)
o.append('<text x="%.2f" y="%.2f" class="lbl" text-anchor="middle">PANTALLA</text>' % (cx - 2, cy - 4.5))
conector([(12, 16), (13, 16), (14, 16), (15, 16)], ['GND', 'NAV', 'SEL', 'ATR'], 'abajo')
conector([(16, 16), (17, 16), (18, 16)], ['S', '+', '-'], 'abajo')
conector([(24, 16), (25, 16)], ['GND', '5V'], 'abajo')
for x0, x1, t in [(12, 15, 'BOTONES'), (16, 18, 'BUZZER'), (24, 25, 'ENTRA 5 V')]:
    a = h0(x0, 16)
    b = h0(x1, 16)
    o.append('<text x="%.2f" y="%.2f" class="lbl" text-anchor="middle">%s</text>' % ((a[0] + b[0]) / 2, OY + FILAS * p + p + 4, t))

# cables por debajo
R, N, AZ, VE, NA, AM, VI, BL, GR = '#d00', '#111', '#1f5fbf', '#2e9d3a', '#e67e22', '#c9a800', '#8e44ad', '#888', '#555'
linea([(X0, YA), (X0, 1)], N, 0.8)                                            # GND (fila de arriba) -> riel GND
linea([(X0, YB), (X0, 18)], R, 0.8)                                           # 3V3 -> riel 3V3
linea([(X0 + 13, YB), (X0 + 13, 15), (11, 15), (11, 16), (12, 16)], N, 0.6)  # GND -> GND de los botones
linea([(13, 16), (13, 15.4), (X0 + 6, 15.4), (X0 + 6, YB)], AZ, 0.5)          # NAV -> G32
linea([(14, 16), (14, 15.7), (X0 + 7, 15.7), (X0 + 7, YB)], VE, 0.5)          # SEL -> G33
linea([(15, 16), (15, 15.9), (X0 + 8, 15.9), (X0 + 8, YB)], NA, 0.5)          # ATR -> G25
linea([(16, 16), (X0 + 9, YB)], GR, 0.5)                                      # buzzer S -> G26
linea([(17, 16), (17, 18)], R, 0.6)                                           # buzzer + -> 3V3
linea([(18, 16), (18, 15.2)], N, 0.6)                                         # buzzer - -> GND
linea([(25, 16), (X0 + 18, YB)], R, 0.8)                                      # 5V entrada -> pin 5V
linea([(24, 16), (24, 15.2), (X0 + 13, 15.2)], N, 0.6)                        # GND entrada -> GND
linea([(4, 4), (0.5, 4), (0.5, 18)], R, 0.6)                                  # pantalla VCC -> 3V3
linea([(4, 11), (1.0, 11), (1.0, 18)], R, 0.6)                                # pantalla LED -> 3V3
linea([(4, 5), (1.5, 5), (1.5, 1)], N, 0.6)                                   # pantalla GND -> GND
linea([(4, 6), (4.7, 6), (4.7, 1.6), (X0 + 9, 1.6), (X0 + 9, YA)], BL, 0.5)   # CS -> G5
linea([(4, 7), (5.1, 7), (5.1, 3.5), (X0 + 2, 3.5), (X0 + 2, YA)], NA, 0.5)   # RESET -> G22
linea([(4, 8), (5.5, 8), (5.5, 2.9), (X0 + 5, 2.9), (X0 + 5, YA)], AM, 0.5)   # DC -> G21
linea([(4, 9), (5.9, 9), (5.9, 3.9), (X0 + 1, 3.9), (X0 + 1, YA)], VE, 0.5)   # MOSI -> G23
linea([(4, 10), (6.3, 10), (6.3, 2.0), (X0 + 8, 2.0), (X0 + 8, YA)], AZ, 0.5) # SCK -> G18
linea([(4, 12), (6.6, 12), (6.6, 2.45), (X0 + 7, 2.45), (X0 + 7, YA)], VI, 0.5) # MISO -> G19
lx, ly = h0(0, 1)
o.append('<text x="%.2f" y="%.2f" class="lbl">RIEL GND (alambre desnudo por abajo)</text>' % (lx, ly - 2.5))
lx, ly = h0(0, 18)
o.append('<text x="%.2f" y="%.2f" class="lbl" style="fill:#d00">RIEL 3V3 (alambre desnudo por abajo)</text>' % (lx, ly + 5))
svg = '\n'.join(o)

pasos = [
    ('t2', 'Cómo se arma, paso a paso'),
    ('tx', '<tspan font-weight="700">1.</tspan> Soldar 2 tiras de 19 pines HEMBRA, separadas'),
    ('tx', '   10 agujeros. La ESP32 se ENCHUFA ahí: no se suelda.'),
    ('tx', '   (Medir: si las filas de mi ESP32 quedan a 9, usar 9.)'),
    ('', ''),
    ('tx', '<tspan font-weight="700">2.</tspan> Soldar tiras de pines MACHO (amarillo) para cada'),
    ('tx', '   cosa: pantalla (9), botones (4), buzzer (3), energía (2).'),
    ('tx', '   Así todo se conecta con cables dupont hembra.'),
    ('', ''),
    ('tx', '<tspan font-weight="700">3.</tspan> Hacer los 2 rieles con alambre desnudo por abajo:'),
    ('tx', '   arriba GND (negro), abajo 3V3 (rojo).'),
    ('', ''),
    ('tx', '<tspan font-weight="700">4.</tspan> Unir con cablecitos por abajo, como las líneas de'),
    ('tx', '   colores. Un cable por vez, tachando en la hoja 3.'),
    ('', ''),
    ('tx', '<tspan font-weight="700">5.</tspan> Antes de enchufar la ESP32: con el tester, que NO'),
    ('tx', '   haya continuidad entre el riel 3V3 y el riel GND.'),
    ('', ''),
    ('tx', '<tspan font-weight="700">6.</tspan> Enchufar la ESP32 con el USB-C a la derecha,'),
    ('tx', '   como en el dibujo, y probar con USB antes de'),
    ('tx', '   conectar la batería.'),
    ('', ''),
    ('t2', 'Ojo'),
    ('tx', '• Los nombres de los pines son los de mi placa, vista'),
    ('tx', '  desde arriba con el USB a la derecha. Revisarlos con'),
    ('tx', '  lo que dice impreso antes de soldar.'),
    ('tx', '• "ENTRA 5 V" es donde llega el cable del step-up. Va al'),
    ('tx', '  pin 5V de la ESP32 (es la entrada VIN).'),
    ('tx', '• Botones: cada uno lleva 2 cables, uno a su pin'),
    ('tx', '  (NAV / SEL / ATR) y otro al GND de ese conector.'),
    ('tx', '• Los cables que se cruzan van aislados (no se tocan).'),
]
texto = []
y = 0
for clase, t in pasos:
    if clase:
        texto.append('<text x="0" y="%.1f" class="%s" xml:space="preserve">%s</text>' % (y, clase, t))
    y += 4.6 if clase else 2.2

pagina = '''
<div class="hoja" id="placa"><svg width="297mm" height="210mm" viewBox="0 0 297 210">
<style>.pin{font-size:2.3px;fill:#222} .lbl{font-size:2.8px;font-weight:700;fill:#2b5d8a}</style>
  <text x="12" y="16" class="t1">5. La placa perforada: qué se suelda y dónde</text>
  <text x="12" y="22" class="tx">Dibujo AGRANDADO al doble (escala 2:1). Placa perforada de 7 × 5 cm (27 × 19 agujeros). Vista desde arriba; las líneas de colores son cablecitos soldados por debajo.</text>
  %s
  <g transform="translate(170,34)">%s</g>
</svg></div>
''' % (svg, '\n'.join(texto))

ruta = sys.argv[1]
s = io.open(ruta, encoding='utf-8').read()
marca = '\n<div class="hoja" id="placa">'
if marca in s:
    s = s[:s.index(marca)] + '\n</body></html>\n'
s = s.replace('</body></html>', pagina + '</body></html>', 1)
io.open(ruta, 'w', encoding='utf-8').write(s)
print('ok')
