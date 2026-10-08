// objetos_juego.h - Corazon y Bomba para el minijuego Atrapar, 16x16
#pragma once
#include <Arduino.h>

#define OBJ_W 16
#define OBJ_H 16

const char PALETA_CORAZON[] = "ABCD";
const uint16_t PALETA_CORAZON_COLORES[] = {0x0000, 0xB882, 0x7082, 0xF514};
const int PALETA_CORAZON_N = 4;
const char* const SPR_CORAZON[OBJ_H] = {
  "................",
  "..AAAA....AAAA..",
  ".ABBBBA..ABBBBA.",
  "ABBCBBBAABBBBBBA",
  "ABDBCBBBBBBBBBBA",
  "ABBBBBBBBBBBBBBA",
  "ABBBBBBBBBBBBBBA",
  ".ABBBBBBBBBBBBA.",
  ".ABBBBBBBBBBBBA.",
  "..ABBBBBBBBBBA..",
  "..ABBBBBBBBBBA..",
  "...ABBBBBBBBA...",
  "....ABBBBBBA....",
  ".....ABBBBA.....",
  "......ABBA......",
  ".......AA.......",
};

const char PALETA_BOMBA[] = "ABCD";
const uint16_t PALETA_BOMBA_COLORES[] = {0x0000, 0xDE42, 0xB882, 0xFFFF};
const int PALETA_BOMBA_N = 4;
const char* const SPR_BOMBA[OBJ_H] = {
  ".........AA.....",
  "........A..BC...",
  ".......AAA..BC..",
  ".....AAAAAAAC...",
  "....AADDAAAAA...",
  "...AAADAAAAAAA..",
  "..AAADAAAAAAAAA.",
  "..AAADAAAAAAAAA.",
  "..AAAAAAAAAAAAA.",
  "..AAAAAAAAAAAAA.",
  "..AAAAAAAAAAAAA.",
  "...AAAAAAAAAAA..",
  "...AAAAAAAAAAA..",
  "....AAAAAAAAA...",
  ".....AAAAAAA....",
  "................",
};