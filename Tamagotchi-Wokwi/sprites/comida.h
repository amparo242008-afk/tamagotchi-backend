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
// Gaseosa y hamburguesa (agregadas el 09/10)
const char PALETA_GASEOSA[] = "ABCDE";
const uint16_t PALETA_GASEOSA_COLORES[] = {0x2080, 0x3100, 0x8082, 0xB800, 0xCCD3};
const int PALETA_GASEOSA_N = 5;
const char* const SPR_GASEOSA[24] = {
  "........................",
  "........................",
  "........................",
  "........................",
  "...........CC...........",
  "..........CDDC..........",
  "..........CDDC..........",
  "..........AAAA..........",
  ".........ABBBBA.........",
  "........ABBBBBBA........",
  ".......ABBBBBBBBA.......",
  ".......ABBBBBBBBA.......",
  ".......ABBBBBBBBA.......",
  ".......ACCCCCCCCA.......",
  ".......ADDDDDDDDA.......",
  ".......AEDEDDDEEA.......",
  ".......ADDDEDDEDA.......",
  ".......ADDDDDDDDA.......",
  ".......ACCCCCCCCA.......",
  ".......ABBBBBBBBA.......",
  ".......ABBBBBBBBA.......",
  ".......ABBBBBBBAA.......",
  "........ABBBBBBA........",
  "........AAAAAAAA........",
};

const char PALETA_HAMBURGUESA[] = "ABCDEF";
const uint16_t PALETA_HAMBURGUESA_COLORES[] = {0x5A01, 0x7740, 0x8AE0, 0xCC84, 0xF800, 0xFFA0};
const int PALETA_HAMBURGUESA_N = 6;
const char* const SPR_HAMBURGUESA[24] = {
  "........................",
  "........................",
  "...DDDDDDDDDDDDDDDDDD...",
  "..DDDDDDDDDCDDDDDDDDDD..",
  ".DDDDDCDDDDDDDDDDDDDDDD.",
  ".DDDDDDDCDDDDCDDDDCDCDDD",
  "DDCDCDDDDDDDDDDDDDDDDDDD",
  "DDDDDDDDDCDDDDDDDCDDDCDD",
  ".DCDDDDDDDDDDDDDDDDDDDDD",
  ".DDDDDDDDDDDDDDDDDCDDDD.",
  "BBBDBBDBBBBBBDDBDDDDDBD.",
  "BBBBBBBBBBBBBBBBBBBBBBB.",
  ".BBBBBBBBBBBBBBBBBBBBBBB",
  "EEEEEEEEEBEEEEEEBBBEEEE.",
  ".EEEEEEEEEEEEEEEEEEEEEE.",
  "FFFFFFFFFFFFFFFFFFFFFFF.",
  ".AAAAAFFFFAAAAAAAAAAAAA.",
  "AAAAAAFFFFAAAAAAAAAAAAAA",
  "AAAAAAAFFAAAAAAAAAAAAAAA",
  ".AAAAAAAFAAAAAAAAAAAAAA.",
  ".DDDDDDDDDDDDDDDDDDDDDD.",
  "DDDDDDDDDDDDDDDDDDDDDDDD",
  "DDDDDDDDDDDDDDDDDDDDDDDD",
  ".DDDDDDDDDDDDDDDDDDDDDD.",
};
