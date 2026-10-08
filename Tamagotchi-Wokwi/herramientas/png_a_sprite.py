# png_a_sprite.py - Convierte sprites PNG de Piskel (24x24, o exportados agrandados x2, x4...)
# en la "grilla de letras" que usa el proyecto.
#
# Uso (desde la carpeta Tamagotchi-Wokwi):
#   python herramientas/png_a_sprite.py imagenes/robertocorriendo1.png imagenes/robertocorriendo2.png
#
# - Si todos los colores son de Roberto, usa su paleta (PALETA_LETRAS de sprites.h).
# - Si no, arma una paleta propia para ese sprite (PALETA_<NOMBRE>), como en comida.h.
# Imprime el codigo C: se copia y se pega en el .h que corresponda (carpeta sprites/).
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from png_a_rle import leer_png, a565  # noqa: E402

# Paleta de Roberto: tiene que coincidir con PALETA_LETRAS / PALETA_COLORES de sprites.h
PALETA_ROBERTO = {0x2966: 'A', 0xFDD7: 'B', 0xDDC7: 'C', 0x4E7E: 'D', 0xFFFF: 'E', 0xD4B2: 'F'}
LADO = 24


def leer_sprite(ruta):
    ancho, alto, filas = leer_png(ruta, con_alfa=True)
    escala = ancho // LADO
    if ancho != alto or escala * LADO != ancho:
        sys.exit(f'{ruta}: mide {ancho}x{alto}, tiene que ser 24x24 o un multiplo (48, 96...)')
    return [[filas[y * escala][x * escala] for x in range(LADO)] for y in range(LADO)]


def nombre_c(ruta):
    return os.path.splitext(os.path.basename(ruta))[0].split('.')[0]


def main():
    if len(sys.argv) < 2:
        sys.exit('Uso: python png_a_sprite.py sprite1.png [sprite2.png ...]')
    for ruta in sys.argv[1:]:
        pixeles = leer_sprite(ruta)
        colores = {a565(*p) for fila in pixeles for p in fila if p is not None}
        nombre = nombre_c(ruta)
        if colores <= set(PALETA_ROBERTO):
            letras = PALETA_ROBERTO
            print(f'// {nombre} (paleta de Roberto)')
        else:
            letras = {c: 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'[i] for i, c in enumerate(sorted(colores))}
            mayus = nombre.upper()
            print(f'const char PALETA_{mayus}[] = "{"".join(letras.values())}";')
            print(f'const uint16_t PALETA_{mayus}_COLORES[] = {{{", ".join("0x%04X" % c for c in letras)}}};')
            print(f'const int PALETA_{mayus}_N = {len(letras)};')
        print(f'const char* const spr_{nombre}[SPR_H] = {{')
        for fila in pixeles:
            print('  "' + ''.join('.' if p is None else letras[a565(*p)] for p in fila) + '",')
        print('};\n')


if __name__ == '__main__':
    main()
