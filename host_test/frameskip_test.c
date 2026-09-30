/**
 * @file frameskip_test.c
 * @brief Test du saut d'affichage adaptatif avec une horloge virtuelle : émulation 5 ms/image, affichage 20 ms.
 *
 * usage : frameskip_test rom.gb
 *
 * Projet  : gb-gnuboy-aka — lecteur Game Boy / Game Boy Color pour la Gamebuino AKA (ESP32-S3).
 * Auteur  : Jicehel (Jicehel-Aka)
 * Licence : voir README.md (cœur gnuboy : GPL v2 (components/gnuboy/COPYING) ; composant gamebuino : LGPL).
 */
#include <stdio.h>
#include <string.h>
#include "gbrt_aka.h"
static uint64_t vt; static uint32_t presented, rep_n, last_skipped; static int last_load; static float last_fps;
static void present(const uint16_t *p, int w, int h, void *u) { (void)p; (void)w; (void)h; (void)u; presented++; vt += 20000; }
static uint8_t buttons(void *u) { (void)u; vt += 5000; return 0; }
static uint64_t now(void *u) { (void)u; return vt; }
static void slp(uint32_t us, void *u) { (void)u; vt += us; }
static void rep(float fps, int load, uint32_t skipped, void *u) { (void)u; rep_n++; last_fps = fps; last_load = load; last_skipped = skipped; }
/* Exécute 600 images avec auto_frameskip activé ou non et affiche combien d'affichages ont été sautés. */
static int run(bool skip, const char *rom) {
    vt = 0; presented = rep_n = last_skipped = 0;
    GbrtAkaHal h; memset(&h, 0, sizeof h);
    h.present_rgb555 = present; h.read_buttons = buttons; h.time_us = now; h.sleep_us = slp; h.report = rep; h.auto_frameskip = skip;
    int rc = gbrt_aka_run(&h, rom, NULL, 600);
    printf("auto_frameskip=%d : rc=%d, affichées %u/600, sautées %u, dernier rapport : %.1f img/s, charge %d%%\n", skip, rc, presented, last_skipped, last_fps, last_load);
    return rc;
}
/* Sans saut : tout est affiché mais la charge dépasse 100 %. Avec saut : environ la moitié des images est affichée, charge < 100 %. */
int main(int argc, char **argv) {
    if (argc < 2) return 2;
    int rc = run(false, argv[1]);
    rc |= run(true, argv[1]);
    return rc;
}
