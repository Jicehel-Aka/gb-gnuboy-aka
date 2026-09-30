/**
 * @file pc_ui.c
 * @brief Dessin du lanceur PC : liste de ROM 320x240 dans un tampon ARGB, avec police 8x8 intégrée.
 *
 * Le tampon est ensuite envoyé à SDL par main_pc.c. Aucune dépendance à SDL ici : le code est testable seul.
 *
 * Projet  : gb-gnuboy-aka — lecteur Game Boy / Game Boy Color pour la Gamebuino AKA (ESP32-S3).
 * Auteur  : Jicehel (Jicehel-Aka)
 * Licence : voir README.md (cœur gnuboy : GPL v2 (components/gnuboy/COPYING) ; composant gamebuino : LGPL).
 */
#include "pc_ui.h"

#include <stdio.h>
#include <string.h>

#include "font8x8_basic.h" /* police 8x8, domaine public (D. Hepper / IBM VGA) */

/* Mise en page et couleurs (ARGB) du lanceur. */
#define ROW_H 10
#define LIST_Y 22
#define BG 0xFF101820u
#define C_TITLE 0xFFFFD84Au
#define C_SEL 0xFFFFFFFFu
#define C_FILE 0xFFA8B0B8u
#define C_DIR 0xFF7FC8FFu
#define C_HINT 0xFF607080u
#define C_WARN 0xFFFF9A3Cu
#define C_BAR 0xFF2A4A6Au

/* Écrit du texte ASCII avec la police 8x8 (les caractères > 127 deviennent '?'). */
static void put_text(uint32_t *fb, int x, int y, const char *s, uint32_t col) {
    for (; *s && x + 8 <= UI_W; ++s, x += 8) {
        unsigned c = (unsigned char)*s;
        if (c > 127) c = '?';
        for (int r = 0; r < 8; ++r) {
            uint8_t bits = font8x8_basic[c][r];
            for (int k = 0; k < 8; ++k)
                if (bits & (1u << k)) fb[(size_t)(y + r) * UI_W + (size_t)(x + k)] = col;
        }
    }
}

/* Rectangle plein, rogné aux bords de l'écran. */
static void fill(uint32_t *fb, int x, int y, int w, int h, uint32_t col) {
    for (int j = y; j < y + h && j < UI_H; ++j)
        for (int i = x; i < x + w && i < UI_W; ++i) fb[(size_t)j * UI_W + (size_t)i] = col;
}

/* Nombre de lignes de liste qui tiennent entre le titre et la ligne d'aide. */
int ui_rows(void) { return (UI_H - LIST_Y - 24) / ROW_H; }

/* Dessine tout l'écran du lanceur : titre, liste (sélection surlignée), barre de défilement, avertissements, aide. */
void ui_draw_browser(uint32_t *fb, const GbrtBrowser *b, const char *status, int rows) {
    for (int i = 0; i < UI_W * UI_H; ++i) fb[i] = BG;

    char head[64];
    snprintf(head, sizeof head, "Game Boy  %.28s", gbrt_browser_rel(b));
    put_text(fb, 8, 6, head, C_TITLE);

    if (b->count == 0) {
        put_text(fb, 8, LIST_Y + 8, "Aucune ROM ici", C_SEL);
        put_text(fb, 8, LIST_Y + 22, "Mets des .gb dans :", C_FILE);
        const char *root = b->root;
        size_t n = strlen(root);
        if (n > 38) root += n - 38;
        put_text(fb, 8, LIST_Y + 34, root, C_DIR);
    }
    for (int i = 0; i < rows && b->top + i < b->count; ++i) {
        const int idx = b->top + i;
        const GbrtEntry *e = &b->entries[idx];
        const int y = LIST_Y + i * ROW_H;
        if (idx == b->sel) fill(fb, 4, y - 1, UI_W - 8, ROW_H, C_BAR);
        char line[48];
        snprintf(line, sizeof line, "%c %.36s%s", idx == b->sel ? '>' : ' ', e->name, e->is_dir ? "/" : "");
        put_text(fb, 8, y, line, idx == b->sel ? C_SEL : (e->is_dir ? C_DIR : C_FILE));
    }
    if (b->count > rows) { /* barre de défilement */
        const int track = rows * ROW_H;
        int h = track * rows / b->count;
        if (h < 6) h = 6;
        int y = LIST_Y + (track - h) * b->top / (b->count - rows);
        fill(fb, UI_W - 5, LIST_Y, 2, track, 0xFF203040u);
        fill(fb, UI_W - 5, y, 2, h, C_DIR);
    }
    if (b->truncated) put_text(fb, 8, UI_H - 24, "Liste tronquee (1024 max)", C_WARN);
    if (status && status[0]) put_text(fb, 8, UI_H - 24, status, C_WARN);
    put_text(fb, 8, UI_H - 12, "Entree:jouer Retour:dossier Echap:quit", C_HINT);
}
