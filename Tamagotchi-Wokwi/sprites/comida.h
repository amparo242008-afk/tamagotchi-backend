// comida.h - Sprites de comida, dibujados por Palito, 24x24
#pragma once
#include <Arduino.h>

// Pancho, galletita, gaseosa, cafe y torta (redibujados el 24/10, de imagenes/<nombre>.png)
const char PALETA_PANCHO[] = "ABCDE";
const uint16_t PALETA_PANCHO_COLORES[] = {0x9986, 0xB46B, 0xB986, 0xED8D, 0xEDE0};
const int PALETA_PANCHO_N = 5;
const char* const SPR_PANCHO[24] = {
  "........................",
  "........................",
  "........................",
  "........................",
  "...............BDBA.....",
  "..............BDCCAA....",
  ".............BDCECCAA...",
  "............BDBCECCCB...",
  "...........BDCCCAECBDB..",
  "..........BDBCEEEECBDB..",
  ".........BDCCCECACBDDB..",
  "........BDCCACECCBDDDB..",
  ".......BDCEEACECBDDDDB..",
  "......BDCCECEECBDDDDB...",
  ".....BDCCCECACBDDDDDB...",
  "....BDCCACECCBDDDDDB....",
  "...BDCEEEECCBDDDDDB.....",
  "..BDCCECACCBDDDDDB......",
  "..BCCCECACBDDDDDB.......",
  "..BCCCECCBDDDDDB........",
  "..BACCACBDDDDDB.........",
  "...AACBBDDDDDB..........",
  "....ABDDDDDDB...........",
  "......BBBBBB............",
};

const char PALETA_GALLETITA[] = "ABCDEF";
const uint16_t PALETA_GALLETITA_COLORES[] = {0x7201, 0x8AA2, 0x8AE3, 0xAB85, 0xABC7, 0xB42A};
const int PALETA_GALLETITA_N = 6;
const char* const SPR_GALLETITA[24] = {
  "........................",
  "........................",
  "........................",
  "........................",
  "........................",
  "........................",
  "........................",
  "...........CCCCCCC......",
  ".........CCFFDDDDDCC....",
  ".......CCDEFBBEEEDDDC...",
  "......CDEEEEBAEDFFFEDC..",
  ".....CEEEFFEEEEEFABEDC..",
  "....CDEFEFBAEEEEEBBEDDC.",
  "....CEEEEDBBEDDDDEEFEDC.",
  "...CEEFDDEEEEEDDEFEEEDC.",
  "...CEEEEEEEFEEEEFFEEFDC.",
  "...CEBAEEFFEEEEEEFEEFDC.",
  "...CFBBEEFBBEEEABEEEFC..",
  "...CFFEEEEBADEEBBEEDDC..",
  "....CEEEEEEEEEEEEEDDC...",
  "....CDEFEEFEEEEEEDDC....",
  ".....CDFBAEFFEDDDCC.....",
  "......CCBBDDDDCCC.......",
  "........CCCCCC..........",
};

const char PALETA_GASEOSA[] = "ABCD";
const uint16_t PALETA_GASEOSA_COLORES[] = {0x3D09, 0x662E, 0x8691, 0xC7F9};
const int PALETA_GASEOSA_N = 4;
const char* const SPR_GASEOSA[24] = {
  "..........BBBB..........",
  ".........BBCCBB.........",
  ".........BCCCCB.........",
  "..........BBBB..........",
  "..........DDDD..........",
  ".........DDCBDD.........",
  ".........DBCBBD.........",
  "........DDBCBBDD........",
  ".......DDBCCBBBDD.......",
  "......AAAAAAAAAAAA......",
  "......ADDADDADDADA......",
  "......ADDDADDDDDDA......",
  "......AADDADDADDAA......",
  ".......AAAAAAAAAA.......",
  ".......DBBCBCCBBD.......",
  ".......DBCCBBCDBD.......",
  ".......DBCCBBCCBD.......",
  ".......DBCBBBBCBD.......",
  ".......DCCBBDBCBD.......",
  "......DBCCBBCBCBBD......",
  "......DBBCBBCCCBBD......",
  "......DBBBBBBBBBBD......",
  "......DBBDDBBDDBBD......",
  ".......DD..DD..DD.......",
};

const char PALETA_CAFE[] = "ABCDE";
const uint16_t PALETA_CAFE_COLORES[] = {0x7201, 0x8B68, 0xA657, 0xAF1A, 0xDF5B};
const int PALETA_CAFE_N = 5;
const char* const SPR_CAFE[24] = {
  "........................",
  "........................",
  "........................",
  "........................",
  "........................",
  "........................",
  "........................",
  "........................",
  "........................",
  ".......EEEEEEEEE........",
  "......ECCCCCCCCCE.......",
  ".....ECCABBBAAADCE......",
  ".....ECABBAAAAAADEEE....",
  ".....EDABAABABBADCDCE...",
  ".....EDCAAABBBACCEDDE...",
  ".....EDCDDDDCCCCCEECE...",
  ".....EDCCCCCCCCCCECCE...",
  ".....EDDCCCCCCCDDCCE....",
  ".....EEDDCCCCCCDEEE.....",
  "....EDCEDDDDDDDECCE.....",
  "....EDDCEEEEEEECCCE.....",
  ".....EEDDCCCCCCCEE......",
  ".......EEEEEEEEE........",
  "........................",
};

const char PALETA_TORTA[] = "ABCDEF";
const uint16_t PALETA_TORTA_COLORES[] = {0xB986, 0xE2CB, 0xE575, 0xEBEF, 0xF6AD, 0xF6F0};
const int PALETA_TORTA_N = 6;
const char* const SPR_TORTA[24] = {
  "........................",
  "........................",
  ".........AAA............",
  "........ABBBA...........",
  ".......ABCBCBA..........",
  "......DABBBBBA..........",
  ".....DABCBCBBBA.........",
  "....DDABBBBBCBA.........",
  "...DDDABBCBCBBAD........",
  "...DCDDABBBBBADDD.......",
  "..DDDDDDAAAAACCDDD......",
  "..DDCDCCDDDDDDDDCDD.....",
  ".DDDDDDDDDCDDCDDCDDD....",
  ".DDDDDCDDDDDDDDDDDDDD...",
  ".EDDDDDDDDDDDDDDDDDDD...",
  ".EFDFEDFFDFFEDFEDDFDEE..",
  ".EFFFEFFFFFFEFFEEFFFEE..",
  ".EFCCCCCCCCCCCCCCCCCCE..",
  ".EFFAAAAFFFFAAAAFFFFFE..",
  ".EFFADDDAAFFADDDAAFFFE..",
  ".EFCCCCCCCCCCCCCCCCCCE..",
  ".EFFFFFFEFFFFFFFFFFFFE..",
  "..EEEEEEEEEEEEEEEEEEE...",
  "........................",
};

// Hamburguesa (del 09/10)
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
