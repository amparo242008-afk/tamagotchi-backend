// sprites.h - Sprites de Chaca (Palito, paleta actualizada), 24x24
#pragma once
#include <Arduino.h>

#define SPR_W 24
#define SPR_H 24

const char PALETA_LETRAS[] = "ABCDEF";
// F = interior de la boca abierta (hablando / bostezando)
const uint16_t PALETA_COLORES[] = {0x2966, 0xFDD7, 0xDDC7, 0x4E7E, 0xFFFF, 0xD4B2};
const int PALETA_N = 6;

// frame 0 de la hoja
const char* const spr_normal0[SPR_H] = {
  "........................",
  "........................",
  "........................",
  "........................",
  "........................",
  "........................",
  ".....A...B..............",
  "......A..BBAA..AA.......",
  ".......BBAAAAAA.........",
  "......AABAAAAAAA........",
  ".....ACCCCCCCCCCA.......",
  ".....ACACCCCCCACA.......",
  ".....ACCCCACCCCCA.......",
  ".....ACCAACAACCCA.......",
  ".....ACCCCCCCCCCA.......",
  "......ACCCCCCCCA.AA.....",
  ".....AACCCCCCCCAA.......",
  "...AA.ACCCCCCCCA........",
  "......AACCCCCCAA........",
  ".......AAAAAAAA.........",
  "........A....A..........",
  "......AA.....AA.........",
  "........................",
  "........................",
};

// frame 1 de la hoja
const char* const spr_normal1[SPR_H] = {
  "........................",
  "........................",
  "........................",
  "........................",
  "........................",
  "........................",
  ".........B..............",
  ".....AA..BBAA..AA.......",
  ".......BBAAAAAA.........",
  "......AABAAAAAAA........",
  ".....ACCCCCCCCCCA.......",
  ".....ACACCCCCCACA.......",
  ".....ACCCCACCCCCA.......",
  ".....ACCAACAACCCA.......",
  ".....ACCCCCCCCCCA.......",
  "......ACCCCCCCCA........",
  ".....AACCCCCCCCAA.......",
  "....A.ACCCCCCCCA.A......",
  "....A..AAAAAAAA..A......",
  "........A....A..........",
  ".......AA.....AA........",
  "........................",
  "........................",
  "........................",
};

// frame 2 de la hoja
const char* const spr_jugar0[SPR_H] = {
  "........................",
  "........................",
  "........................",
  "........................",
  "........................",
  ".....A...B..............",
  "......A..BBAA..AA.......",
  ".......BBAAAAAA.........",
  "......AABAAAAAAA........",
  ".....ACCCCCCCCCCA.......",
  ".....ACAACCCCAACA.......",
  ".....ACCCCACCCCCA.......",
  ".....ACCAAAAACCCA.......",
  ".....ACCCABACCCCA.......",
  "...A.ACCCAAACCCCA.A.....",
  "....A.ACCCCCCCCA.A......",
  ".....AACCCCCCCCAA.......",
  "......ACCCCCCCCA........",
  "......AACCCCCCAA........",
  ".......AAAAAAAA.........",
  "........A....A..........",
  "......AA.....AA.........",
  "........................",
  "........................",
};

// frame 3 de la hoja
const char* const spr_hambre0[SPR_H] = {
  "........................",
  "........................",
  "........................",
  "........................",
  "........................",
  "........................",
  ".........B..............",
  ".....AA..BBAA..AA.......",
  ".......BBAAAAAA.........",
  "......AABAAAAAAA........",
  ".....ACCCCCCCCCCA.......",
  ".....ACCACCCCACCA.......",
  ".....ACDDCCCCDDCA.......",
  ".....ACDCCAACCDCA.......",
  ".....ACCCACCACCCA.......",
  "......ACCCCCCCCA........",
  ".....AACCCCCCCCAA.......",
  "....A.ACCCCCCCCA.A......",
  "....A..AAAAAAAA..A......",
  "........A....A..........",
  ".......AA.....AA........",
  "........................",
  "........................",
  "........................",
};

// frame 4 de la hoja
const char* const spr_comer0[SPR_H] = {
  "........................",
  "........................",
  "........................",
  "........................",
  "........................",
  "........................",
  ".............A..........",
  "........BAAAA...A.......",
  ".......BBAAAAA.A........",
  "......AAAAAAAAA.........",
  ".....ACCCCCCCCCA........",
  ".....ACCAACCCCCA........",
  ".....AACCCCCCCCA........",
  ".....AAACCCCCCCA........",
  ".....ACCCCCCCCCA........",
  "......ACCCCCCCA.........",
  ".....AACCCCCCAAA........",
  "......ACCCCCCCA.A.......",
  "......ACCCCCCCA.........",
  ".......AAAAAAA..........",
  "........A...A...........",
  ".......AA..AA...........",
  "........................",
  "........................",
};


// frame 6 de la hoja
const char* const spr_comer2[SPR_H] = {
  "........................",
  "........................",
  "........................",
  "........................",
  "........................",
  "........................",
  "........................",
  ".....AA.BAAAA.AA........",
  ".......BBAAAAA..........",
  "......AAAAAAAAA.........",
  ".....ACCCCCCCCCA........",
  ".....ACCAACCCCCA........",
  ".....AACCCCCCCCA........",
  ".....AAACCCCCCCA........",
  ".....ACCCCCCCCCA........",
  "......ACCCCCCCA.........",
  "......ACCCAACCA.........",
  "......ACCAACCCA.........",
  "......ACCCCCCCA.........",
  ".......AAAAAAA..........",
  "..........A.A...........",
  ".........AAAA...........",
  "........................",
  "........................",
};

// frame 7 de la hoja
const char* const spr_comer3[SPR_H] = {
  "........................",
  "........................",
  "........................",
  "........................",
  "........................",
  "........................",
  ".....AA.BAAAA.AA........",
  ".......BBAAAAA..........",
  "......AAAAAAAAA.........",
  ".....ACCCCCCCCCA........",
  ".....ACCACCCCCCA........",
  ".....AACCCCCCCCA........",
  ".....ABAACCCCCCA........",
  ".....ABACCCCCCCA........",
  ".....AAACCCCCCCA........",
  "......ACCCCCCCA.........",
  "......ACCCAACCA.........",
  "......ACCAACCCA.........",
  "......ACCCCCCCA.........",
  ".......AAAAAAA..........",
  "..........A.A...........",
  ".........AAAA...........",
  "........................",
  "........................",
};

// frame 8 de la hoja
const char* const spr_dormir0[SPR_H] = {
  "........................",
  "........................",
  ".......EEEE.............",
  "......EAAAAE............",
  ".......EEEAE............",
  "........EAE.............",
  ".......EAE..............",
  "......EAEEE.............",
  "......EAAAAE............",
  ".......EEEE.............",
  "..........AAAAA......A..",
  ".......A.ACCCCCAAAA..A..",
  "......A.AACACCCACCCAA...",
  ".......AAACACCCCACCAA...",
  ".......AAACCCCACACCA....",
  ".......BAACCCACCCCCA....",
  ".......BBACACCACCCCA....",
  ".......ABACACCCCACCA.A..",
  "......A.AACCCCCAACCAA...",
  "......A..AAAAAAAAAA.....",
  "........................",
  "........................",
  "........................",
  "........................",
};

// frame 9 de la hoja
const char* const spr_dormir1[SPR_H] = {
  ".........EEEE...........",
  "........EAAAAE..........",
  ".........EEEAE..........",
  "..........EAE...........",
  ".........EAE............",
  "........EAEEE...........",
  "........EAAAAE..........",
  ".........EEEEE..........",
  "........................",
  "........................",
  "..........AAAAA......A..",
  ".......A.ACCCCCAAAA..A..",
  "......A.AACACCCACCCAA...",
  ".......AAACACCCCACCA....",
  ".......AAACCCCACACCA....",
  ".......BAACCCACCCCCA....",
  ".......BBACACCACCCCA....",
  ".......ABACACCCCACCA.A..",
  "......A.AACCCCCAACCAA...",
  "......A..AAAAAAAAAA.....",
  "........................",
  "........................",
  "........................",
  "........................",
};

// ---- Sprites nuevos (convertidos con herramientas/png_a_sprite.py) ----
// robertocorriendo1 (paleta de Chaca)
const char* const spr_robertocorriendo1[SPR_H] = {
  "........................",
  "........................",
  "........................",
  "........................",
  "........................",
  "........................",
  ".............A..........",
  "........BAAAA...A.......",
  ".......BBAAAAA.A........",
  "......AAAAAAAAA.........",
  ".....ACCCCCCCCCA........",
  ".....ACCAACCCCCA........",
  ".....AACCCCCCCCA........",
  ".....AAACCCCCCCA........",
  ".....ACCCCCCCCCA........",
  "......ACCCCCCCA.........",
  ".....AACCCCCCAAA........",
  "......ACCCCCCCA.A.......",
  "......ACCCCCCCA.........",
  ".......AAAAAAA..........",
  "........A...A...........",
  ".......AA..AA...........",
  "........................",
  "........................",
};

// robertocorriendo2 (paleta de Chaca)
const char* const spr_robertocorriendo2[SPR_H] = {
  "........................",
  "........................",
  "........................",
  "........................",
  "........................",
  "........................",
  "........................",
  ".....AA.BAAAA.AA........",
  ".......BBAAAAA..........",
  "......AAAAAAAAA.........",
  ".....ACCCCCCCCCA........",
  ".....AACCAACCCCA........",
  ".....ACACCCCCCCA........",
  ".....AACAACCCCCA........",
  ".....ACCCCCCCCCA........",
  "......ACCCCCCCA.........",
  ".....AACCCCACCA.........",
  "....A.ACCAACCCA.........",
  "......ACCCCCCCA.........",
  ".......AAAAAAA..........",
  ".....AA......AA.........",
  "..............A.........",
  "........................",
  "........................",
};

// robertofestejo (paleta de Chaca)
const char* const spr_robertofestejo[SPR_H] = {
  "........................",
  "........................",
  "........................",
  "........................",
  "........................",
  ".....A...B..............",
  "......A..BBAA..AA.......",
  ".......BBAAAAAA.........",
  "......AABAAAAAAA........",
  ".....ACCCCCCCCCCA.......",
  ".....ACAACCCCAACA.......",
  ".....ACCCCACCCCCA.......",
  ".....ACCAAAAACCCA.......",
  ".....ACCCABACCCCA.......",
  "...A.ACCCAAACCCCA.A.....",
  "....A.ACCCCCCCCA.A......",
  ".....AACCCCCCCCAA.......",
  "......ACCCCCCCCA........",
  "......AACCCCCCAA........",
  ".......AAAAAAAA.........",
  "........A....A..........",
  "......AA.....AA.........",
  "........................",
  "........................",
};

// robertohablando1 (paleta de Chaca)
const char* const spr_robertohablando1[SPR_H] = {
  "........................",
  "........................",
  "........................",
  "........................",
  "........................",
  "........................",
  ".....A...B..............",
  "......A..BBAA..AA.......",
  ".......BBAAAAAA.........",
  "......AABAAAAAAA........",
  ".....ACCCCCCCCCCA.......",
  ".....ACACCCCCCACA.......",
  ".....ACCCCACCCCCA.......",
  ".....ACCAACAACCCA.......",
  ".....ACCCCCCCCCCA.......",
  "...A..ACCCCCCCCA.AA.....",
  "....AAACCCCCCCCAA.......",
  "......ACCCCCCCCA........",
  "......AACCCCCCAA........",
  ".......AAAAAAAA.........",
  "........A....A..........",
  "......AA.....AA.........",
  "........................",
  "........................",
};

// robertohablando3 (paleta de Chaca)
const char* const spr_robertohablando3[SPR_H] = {
  "........................",
  "........................",
  "........................",
  "........................",
  "........................",
  "........................",
  ".....A...B..............",
  "......A..BBAA..AA.......",
  ".......BBAAAAAA.........",
  "......AABAAAAAAA........",
  ".....ACCCCCCCCCCA.......",
  ".....ACACCACCCACA.......",
  ".....ACCAAAAACCCA.......",
  ".....ACCCABACCCCA.......",
  ".....ACCCABACCCCA.......",
  "....A.ACCAAACCCA.A......",
  "....AAACCCCCCCCAAA......",
  "......ACCCCCCCCA........",
  "......AACCCCCCAA........",
  ".......AAAAAAAA.........",
  "........A....A..........",
  "......AA.....AA.........",
  "........................",
  "........................",
};

// robertosaludo1 (paleta de Chaca)
const char* const spr_robertosaludo1[SPR_H] = {
  "........................",
  "........................",
  "........................",
  "........................",
  "........................",
  "........................",
  ".....A...B..............",
  "......A..BBAA..AA.......",
  ".......BBAAAAAA.........",
  "......AABAAAAAAA........",
  ".....ACCCCCCCCCCA.......",
  ".....ACACCACCCACA.......",
  ".....ACCAAAAACCCA.......",
  ".....ACCCABACCCCA.......",
  ".....ACCCAAACCCCA.......",
  "......ACCCCCCCCA........",
  ".....AACCCCCCCCAA.......",
  "...AA.ACCCCCCCCA.AA.....",
  "......AACCCCCCAA........",
  ".......AAAAAAAA.........",
  "........A....A..........",
  "......AA.....AA.........",
  "........................",
  "........................",
};

// robertosaludo2 (paleta de Chaca)
const char* const spr_robertosaludo2[SPR_H] = {
  "........................",
  "........................",
  "........................",
  "........................",
  "........................",
  "........................",
  ".....A...B..............",
  "......A..BBAA..AA.......",
  ".......BBAAAAAA.........",
  "......AABAAAAAAA........",
  ".....ACCCCCCCCCCA.......",
  ".....ACACCACCCACA.......",
  ".....ACCAAAAACCCA.......",
  ".....ACCCABACCCCA.......",
  "...A.ACCCABACCCCA.A.....",
  "....A.ACCAAACCCA.A......",
  ".....AACCCCCCCCAA.......",
  "......ACCCCCCCCA........",
  "......AACCCCCCAA........",
  ".......AAAAAAAA.........",
  "........A....A..........",
  "......AA.....AA.........",
  "........................",
  "........................",
};

// robertoaburrido (paleta de Chaca)
const char* const spr_robertoaburrido[SPR_H] = {
  "........................",
  "........................",
  "........................",
  "........................",
  "........................",
  "........................",
  ".........B..............",
  ".....AA..BBAA..AA.......",
  "....A..BBAAAAAA..A......",
  "......AABAAAAAAA........",
  ".....ACCCCCCCCCCA.......",
  ".....AAACCCCAACCA.......",
  ".....ACCCACCCCCCA.......",
  ".....ACCAAACCCCCA.......",
  ".....ACACCCACCCCA.......",
  "......ACCCCCCCCA........",
  "....AAACCCCCCCCAAA......",
  "...A..ACCCCCCCCA..A.....",
  ".......AAAAAAAA.........",
  "........A...AA..........",
  ".......AA..AA...........",
  "........................",
  "........................",
  "........................",
};

// robertobostesando (paleta de Chaca)
const char* const spr_robertobostesando[SPR_H] = {
  "........................",
  "........................",
  "........................",
  "........................",
  "........................",
  "........................",
  ".........B..............",
  ".....AA..BBAA..AA.......",
  ".......BBAAAAAA.........",
  "......AABAAAAAAA........",
  ".....ACCCCCCCCCCA.......",
  ".....ACAACCCCAACA.......",
  ".....ACCCCACCCCCA.......",
  ".....ACCAAFAACCCA.......",
  ".....ACCCAFACCCCA.......",
  "......ACCCACCACA........",
  ".....AACCCCCCCAA........",
  "....A.ACCCCCCCCA........",
  "....A..AAAAAAAA.........",
  "........A....A..........",
  ".......AA...AA..........",
  "........................",
  "........................",
  "........................",
};

const char* const* const SPR_NORMAL[] = {spr_normal0, spr_normal1};
#define N_NORMAL 2
const char* const* const SPR_JUGAR[] = {spr_jugar0};
#define N_JUGAR 1
const char* const* const SPR_HAMBRE[] = {spr_hambre0};
#define N_HAMBRE 1
const char* const* const SPR_COMER[] = {spr_comer0, spr_comer2, spr_comer3};
#define N_COMER 3
const char* const* const SPR_DORMIR[] = {spr_dormir0, spr_dormir1};
#define N_DORMIR 2
const char* const* const SPR_CORRER[] = {spr_robertocorriendo1, spr_robertocorriendo2};
#define N_CORRER 2
const char* const* const SPR_FESTEJO[] = {spr_robertofestejo};
#define N_FESTEJO 1
const char* const* const SPR_HABLAR[] = {spr_robertohablando1, spr_robertohablando3};
#define N_HABLAR 2
const char* const* const SPR_SALUDO[] = {spr_robertosaludo1, spr_robertosaludo2};
#define N_SALUDO 2
const char* const* const SPR_ABURRIDO[] = {spr_robertoaburrido};
#define N_ABURRIDO 1
const char* const* const SPR_BOSTEZO[] = {spr_robertobostesando};
#define N_BOSTEZO 1

// Chaca en la banadera del fondo del bano: se le saco la banadera propia,
// quedan el cuerpo hasta la cintura, las gotitas y la espuma (filas de abajo).
// Paleta propia: tiene el agua y la banadera. Los dos dibujos usan la misma.
const char PALETA_BANO[] = "ABCDEFGH";
const uint16_t PALETA_BANO_COLORES[] = {0x222A, 0x2966, 0x469E, 0x4E1D, 0xB6DA, 0xD79E, 0xDDC7, 0xFDD7};
const int PALETA_BANO_N = 8;
const char* const spr_robertobano1[SPR_H] = {
  "...................D....",
  ".D..D.....H........D.D..",
  "D..D..B...HH......D..D..",
  "D...D..BHHBBBB..BB....D.",
  ".D.D....BHBBBBBB.....D..",
  "...D...BBBBBBBBBB..D....",
  "......BGGGGGGGGGGB......",
  "......BGBGGGGGGBGB......",
  "......BGGGGBGGGGGB......",
  "......BGGBBGBBGGGB......",
  "......BGGGGGGGGGGB......",
  ".......BGGGGGGGGB.BB....",
  "......BBGGGGGGGGBB......",
  "....BB.BGGGGGGGGB.......",
  "........................",
  "...FF...............F...",
  "..FFF....FF...F....FF...",
  "........FF...F.F....F...",
  "........................",
  "........................",
  "........................",
  "........................",
  "........................",
  "........................",
};
const char* const spr_robertobano2[SPR_H] = {
  "........................",
  "....D.....H........D....",
  "D..D..B...HH......D..D..",
  "D...D..BHHBBBB..BB....D.",
  ".D.D....BHBBBBBB.....D..",
  "...D...BBBBBBBBBB..D....",
  ".D....BGGGGGGGGGGB...D..",
  ".DD...BGBGGGGGGBGB...D..",
  "..D...BGGGGBGGGGGB......",
  "......BGGBBBBBGGGB......",
  "....B.BGGGBHBGGGGB.BB...",
  ".....B.BGGBBBGGGB.B.....",
  "......BBGGGGGGGGBB......",
  ".......BGGGGGGGGB.......",
  "........................",
  "...F................F...",
  "..FFF..F..FF.....FF.FF..",
  "..F...F....F.....F..F...",
  ".F.....F.............F..",
  "........................",
  "........................",
  "........................",
  "........................",
  "........................",
};
const char* const* const SPR_BANO[] = {spr_robertobano1, spr_robertobano2};
#define N_BANO 2
