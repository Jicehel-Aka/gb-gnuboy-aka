/**
 * @file pc_ui.h
 * @brief Interface du dessin du lanceur PC (taille 320x240, nombre de lignes, dessin du navigateur).
 *
 * Projet  : gb-gnuboy-aka — lecteur Game Boy / Game Boy Color pour la Gamebuino AKA (ESP32-S3).
 * Auteur  : Jicehel (Jicehel-Aka)
 * Licence : voir README.md (cœur gnuboy : GPL v2 (components/gnuboy/COPYING) ; composant gamebuino : LGPL).
 */
#ifndef PC_UI_H
#define PC_UI_H
#include <stdint.h>
#include "gbrt_browser.h"

#define UI_W 320
#define UI_H 240

/* Dessine le navigateur dans un tampon ARGB 320x240. status : message (ou NULL). */
void ui_draw_browser(uint32_t *fb, const GbrtBrowser *b, const char *status, int rows);
/* Nombre de lignes de liste affichables. */
int ui_rows(void);
#endif
