// comida.h - Sprites de comida (leche y torta), dibujados por Palito, 24x24
#pragma once
#include <Arduino.h>

const char PALETA_LECHE[] = "ABCD";
const uint16_t PALETA_LECHE_COLORES[] = {0x0000, 0xDBEF, 0xE73C, 0xFE18};
const int PALETA_LECHE_N = 4;
const char* const SPR_LECHE[24] = {
  "........................",
  "........................",
  "........................",
  ".........AAAAAA.........",
  ".........ABBBBA.........",
  "........AAAAAAAA........",
  "......AABBABBABBAA......",
  "......ABBAABBABBBA......",
  ".......AACAAACAAA.......",
  ".......ACCCCCCCCA.......",
  ".......ACCCCCCCCA.......",
  ".......ACCCCCCCCA.......",
  ".......ADDDDDCCCA.......",
  ".......ADCCDDDDDA.......",
  ".......ADCDCCDCCA.......",
  ".......ADDDDCDDCA.......",
  ".......ACCCDDDDDA.......",
  ".......ACCCCCCCCA.......",
  ".......ACCCCCCCCA.......",
  ".......ACCCCCCCCA.......",
  "........ACCCCCCA........",
  ".........AAAAAA.........",
  "........................",
  "........................",
};

const char PALETA_TORTA[] = "ABCD";
const uint16_t PALETA_TORTA_COLORES[] = {0x0000, 0xDBEF, 0xFE18, 0xFF16};
const int PALETA_TORTA_N = 4;
const char* const SPR_TORTA[24] = {
  "........................",
  "........................",
  "........................",
  "........................",
  "........................",
  "...........AA...........",
  "..........ABBA..........",
  ".........ABCBBAAA.......",
  ".......AABCBBBBDDA......",
  ".....AADDBBBBBBDDDA.....",
  "...AADDDDDBBBBDDDDDA....",
  "..ADDDDDDDDDDDDDDDDA....",
  "..ADBBBBBBBBBBBBBBDA....",
  "..ADBBBBBBBBBBBBBBDA....",
  "..ADBBBBBBBBBBBBBBDA....",
  "..ADBBCBBBBCBBBBCBDA....",
  "..ADDCCCDDCCCDDCCCDA....",
  "..ADCCCCCCCCCCCCCCCA....",
  "..ADCCCCCCCCCCCCCCCA....",
  "..ADDBBBBBBBBBBBBDDA....",
  "...ADBBBBBBBBBBBBDA.....",
  "...ADBBBBBBBBBBBBDA.....",
  "....AAAAAAAAAAAAAA......",
  "........................",
};