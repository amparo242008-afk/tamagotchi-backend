# png_a_rle.py - Convierte un fondo PNG (240x320, exportado de Piskel) en un .h comprimido con RLE
#
# Uso (desde la carpeta Tamagotchi-Wokwi):
#   python herramientas/png_a_rle.py imagenes/FondoMenu.png fondos/fondo_menu.h FONDO_MENU
#
# Genera FONDO_MENU_COLOR, FONDO_MENU_CONTEO, FONDO_MENU_RUNS, FONDO_MENU_ANCHO y FONDO_MENU_ALTO,
# igual que los otros fondos. No necesita instalar nada (solo Python).
# Consejo: colores planos, sin degrades -> menos "runs" -> archivo mas chico.
import struct
import sys
import zlib


def leer_png(ruta, con_alfa=False):
    # con_alfa=True: los pixeles transparentes vuelven como None
    datos = open(ruta, 'rb').read()
    if datos[:8] != b'\x89PNG\r\n\x1a\n':
        sys.exit('No es un PNG: ' + ruta)
    i, idat, paleta = 8, b'', None
    while i + 8 <= len(datos):
        n = struct.unpack('>I', datos[i:i + 4])[0]
        tipo, cuerpo = datos[i + 4:i + 8], datos[i + 8:i + 8 + n]
        i += 12 + n
        if tipo == b'IHDR':
            ancho, alto, bits, tipo_color, _, _, entrelazado = struct.unpack('>IIBBBBB', cuerpo)
        elif tipo == b'PLTE':
            paleta = [tuple(cuerpo[j:j + 3]) for j in range(0, len(cuerpo), 3)]
        elif tipo == b'IDAT':
            idat += cuerpo
        elif tipo == b'IEND':
            break
    if bits != 8 or entrelazado:
        sys.exit('PNG no soportado (tiene que ser 8 bits, sin entrelazar)')
    bpp = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[tipo_color]
    crudo = zlib.decompress(idat)
    paso = ancho * bpp
    filas, anterior, p = [], bytearray(paso), 0
    for _ in range(alto):
        filtro, linea = crudo[p], bytearray(crudo[p + 1:p + 1 + paso])
        p += 1 + paso
        for x in range(paso):
            a = linea[x - bpp] if x >= bpp else 0
            b = anterior[x]
            c = anterior[x - bpp] if x >= bpp else 0
            if filtro == 1:
                linea[x] = (linea[x] + a) & 255
            elif filtro == 2:
                linea[x] = (linea[x] + b) & 255
            elif filtro == 3:
                linea[x] = (linea[x] + (a + b) // 2) & 255
            elif filtro == 4:
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                linea[x] = (linea[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        anterior = linea
        fila = []
        for x in range(ancho):
            px = linea[x * bpp:(x + 1) * bpp]
            if tipo_color == 3:
                fila.append(paleta[px[0]])
            elif tipo_color in (0, 4):
                fila.append((px[0],) * 3)
            elif tipo_color == 6 and con_alfa and px[3] < 128:
                fila.append(None)
            else:
                fila.append(tuple(px[:3]))
        filas.append(fila)
    return ancho, alto, filas


def a565(r, g, b):
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__ or 'Uso: python png_a_rle.py imagen.png salida.h NOMBRE')
    entrada, salida, nombre = sys.argv[1], sys.argv[2], sys.argv[3].upper()
    ancho, alto, filas = leer_png(entrada)

    colores, conteos = [], []
    for fila in filas:
        for px in fila:
            c = a565(*px)
            if colores and colores[-1] == c and conteos[-1] < 65535:
                conteos[-1] += 1
            else:
                colores.append(c)
                conteos.append(1)

    def bloque(valores, formato):
        lineas = []
        for i in range(0, len(valores), 13):
            lineas.append('  ' + ', '.join(formato(v) for v in valores[i:i + 13]) + ',')
        return '\n'.join(lineas)

    with open(salida, 'w') as f:
        f.write(f'// {salida} - Fondo ({ancho}x{alto}) comprimido con RLE, generado desde {entrada}\n')
        f.write('#pragma once\n#include <Arduino.h>\n\n')
        f.write(f'#define {nombre}_ANCHO {ancho}\n#define {nombre}_ALTO {alto}\n#define {nombre}_RUNS {len(colores)}\n\n')
        f.write(f'const uint16_t {nombre}_COLOR[{nombre}_RUNS] PROGMEM = {{\n{bloque(colores, lambda v: "0x%04X" % v)}\n}};\n\n')
        f.write(f'const uint16_t {nombre}_CONTEO[{nombre}_RUNS] PROGMEM = {{\n{bloque(conteos, str)}\n}};\n')
    print(f'{salida}: {ancho}x{alto}, {len(colores)} runs (~{len(colores) * 4 // 1024} KB)')


if __name__ == '__main__':
    main()
