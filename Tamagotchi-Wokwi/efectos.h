// efectos.h - Burbuja de dialogo y estrella de festejo (24x24, paleta propia cada uno)
#pragma once
#include <Arduino.h>
#include "sprites.h"

const char PALETA_BURBUJATEXTO[] = "AB";
const uint16_t PALETA_BURBUJATEXTO_COLORES[] = {0x9DB8, 0xBEDD};
const int PALETA_BURBUJATEXTO_N = 2;
const char* const spr_burbujatexto[SPR_H] = {
  "........................",
  "........................",
  "........................",
  "........................",
  ".......AAAAAAAAAAA......",
  "......ABBBBBBBBBBBAA....",
  ".....ABBBBBBBBBBBBBBA...",
  "....ABBBBBBBBBBBBBBBBA..",
  "...ABBBBBBBBBBBBBBBBBBA.",
  "...ABBBBBBBBBBBBBBBBBBA.",
  "..ABBBBBBBBBBBBBBBBBBBA.",
  "..ABBBBBBBBBBBBBBBBBBBA.",
  "..ABBBBBBBBBBBBBBBBBBBA.",
  "..ABBBBBBBBBBBBBBBBBBBA.",
  "...ABBBBBBBBBBBBBBBBBBA.",
  "....ABBBBBBBBBBBBBBBBA..",
  "....ABBBBBBBBBBBBBBBA...",
  ".....ABBBBBBBBBBBBBA....",
  "......AAAAAAAAAAAAA.....",
  ".....AA.................",
  "....AA..................",
  "........................",
  "........................",
  "........................",
};

const char PALETA_ESTRELLAFESTEJO[] = "AB";
const uint16_t PALETA_ESTRELLAFESTEJO_COLORES[] = {0xF643, 0xF6CD};
const int PALETA_ESTRELLAFESTEJO_N = 2;
const char* const spr_estrellafestejo[SPR_H] = {
  "........................",
  "........................",
  "............A...........",
  "............A...........",
  "...........AAA..........",
  "...........ABA..........",
  "..........ABAAA.........",
  "..........ABAAA.........",
  ".........AABAAAA........",
  ".........ABAAAAA........",
  "........AABAAAAAA.......",
  "........AAAAAAAAA.......",
  "........AAAAAAAAA.......",
  "........AAAAAAAAA.......",
  "........AAAAAAAAA.......",
  "....A....AAAAAAA........",
  "...ABA...AAAAAAA........",
  "...ABA....AAAAA.........",
  "..ABAAA...AAAAA.........",
  "..ABAAA....AAA..........",
  "..AAAAA....AAA..........",
  "...AAA......A...........",
  "...AAA......A...........",
  "....A...................",
};

