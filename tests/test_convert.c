/**
 * @file test_convert.c
 * @brief Test exhaustif des conversions de couleur : RGB555 -> BGR565/RGB565 doit égaler le chemin RGBA.
 *
 * Les 32768 couleurs RGB555 passent par les deux chemins ; tout écart fait échouer le test.
 *
 * Projet  : gb-gnuboy-aka — lecteur Game Boy / Game Boy Color pour la Gamebuino AKA (ESP32-S3).
 * Auteur  : Jicehel (Jicehel-Aka)
 * Licence : voir README.md (cœur gnuboy : GPL v2 (components/gnuboy/COPYING) ; composant gamebuino : LGPL).
 */
#include <stdio.h>
#include <stdint.h>
#include "gbrt_aka.h"
/* Compare, pour chaque couleur, le chemin direct RGB555 et le chemin RGBA de référence. */
int main(void){ int bad=0; for(uint32_t v=0; v<32768; v++){ uint16_t s=(uint16_t)v, o;
  uint8_t r=(uint8_t)(((v>>0)&31)*255/31), g=(uint8_t)(((v>>5)&31)*255/31), b=(uint8_t)(((v>>10)&31)*255/31);
  uint32_t rgba=0xFF000000u|((uint32_t)r<<16)|((uint32_t)g<<8)|b; uint16_t ref; gbrt_aka_rgba_to_bgr565(&rgba,&ref,1);
  gbrt_aka_rgb555_to_bgr565(&s,&o,1); if(o!=ref) bad++;
  uint16_t r565; gbrt_aka_rgba_to_rgb565(&rgba,&r565,1); uint16_t o2; gbrt_aka_rgb555_to_rgb565(&s,&o2,1); if(o2!=r565) bad++; }
  printf("RGB555 -> BGR565/RGB565 : %d écart(s) sur 32768 couleurs\n",bad); return bad!=0; }
