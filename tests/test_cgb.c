/**
 * @file test_cgb.c
 * @brief Test du mode couleur (CGB) : exécute la ROM synthétique de tools/make_cgb_test_rom.py et vérifie des pixels précis.
 *
 * usage : test_cgb rom.gbc
 *
 * Projet  : gb-gnuboy-aka — lecteur Game Boy / Game Boy Color pour la Gamebuino AKA (ESP32-S3).
 * Auteur  : Jicehel (Jicehel-Aka)
 * Licence : voir README.md (cœur gnuboy : GPL v2 (components/gnuboy/COPYING) ; composant gamebuino : LGPL).
 */
#include <stdio.h>
#include <string.h>
#include "gbrt_aka.h"

static uint16_t last[GBRT_AKA_SCREEN_W * GBRT_AKA_SCREEN_H];
static uint32_t frames;
static void present(const uint16_t *p, int w, int h, void *u) { (void)u; memcpy(last, p, (size_t)w * h * 2); frames++; }
static uint8_t buttons(void *u) { (void)u; return 0; }
static uint64_t now(void *u) { (void)u; return 0; }
static void slp(uint32_t us, void *u) { (void)us; (void)u; }

static int fails; /* nombre d'écarts constatés */
/* Vérifie la couleur RGB555 du pixel (x, y) de la dernière image. */
static void expect(const char *what, int x, int y, uint16_t want) {
    uint16_t got = last[y * GBRT_AKA_SCREEN_W + x] & 0x7FFF;
    if (got != want) { printf("ECHEC %-22s (%3d,%3d) : 0x%04X au lieu de 0x%04X\n", what, x, y, got, want); fails++; }
    else printf("ok    %-22s 0x%04X\n", what, got);
}

/* Lance 120 images de la ROM de test, puis contrôle une couleur par fonctionnalité CGB (palettes, banques VRAM/WRAM, double vitesse, sprite). */
int main(int argc, char **argv) {
    if (argc < 2) return 2;
    GbrtAkaHal h; memset(&h, 0, sizeof h);
    h.present_rgb555 = present; h.read_buttons = buttons; h.time_us = now; h.sleep_us = slp;
    int rc = gbrt_aka_run(&h, argv[1], NULL, 120);
    if (rc != GBRT_AKA_OK || frames != 120) { printf("ECHEC : rc=%d frames=%u\n", rc, frames); return 1; }
    expect("palette 0 (rouge)",            10,  10, 0x001F);
    expect("palette 1 (jaune)",            10,  50, 0x03FF);
    expect("palette 2 (cyan)",             10,  80, 0x7FE0);
    expect("VRAM banque 1 (violet)",       10, 110, 0x4010); /* orange 0x01FF si la banque 1 est ignorée */
    expect("WRAM banques (vert)",          10, 132, 0x0380);
    expect("double vitesse (vert)",        10, 140, 0x0380);
    expect("sprite, palette d'objets",     84,  68, 0x7FFF);
    expect("fond a cote du sprite",        84,  60, 0x03FF);
    printf(fails ? "%d ECHEC(S)\n" : "mode couleur : tous les tests passent\n", fails);
    return fails != 0;
}
