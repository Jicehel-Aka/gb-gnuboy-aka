/**
 * @file host_test.c
 * @brief Banc d'essai PC (sans SDL) : exécute une ROM N images, mesure la vitesse, écrit une capture PPM.
 *
 * Sert à valider la couche gbrt_aka hors de la console.
 * usage : host_test rom.gb frames [dump_frame out.ppm] [save_dir]
 *
 * Projet  : gb-gnuboy-aka — lecteur Game Boy / Game Boy Color pour la Gamebuino AKA (ESP32-S3).
 * Auteur  : Jicehel (Jicehel-Aka)
 * Licence : voir README.md (cœur gnuboy : GPL v2 (components/gnuboy/COPYING) ; composant gamebuino : LGPL).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "gbrt_aka.h"

/* État du banc : compteur d'images, script de touches, statistiques son, dernière image. */
typedef struct {
    uint32_t frame, dump_frame;
    const char *ppm;
    uint8_t script[16][2]; /* {frame_debut, boutons} */
    int script_n;
    uint64_t audio_frames;
    int16_t audio_peak;
    uint64_t fb_hash;
    uint16_t last[GBRT_AKA_SCREEN_W * GBRT_AKA_SCREEN_H];
} T;

/* Mémorise l'image (et l'écrit en PPM à l'image demandée). */
static void present(const uint16_t *px, int w, int h, void *u) {
    T *t = (T *)u;
    memcpy(t->last, px, (size_t)w * h * 2);
    t->frame++;
    if (t->ppm && t->frame == t->dump_frame) {
        FILE *f = fopen(t->ppm, "wb");
        fprintf(f, "P6\n%d %d\n255\n", w, h);
        for (int i = 0; i < w * h; i++) {
            uint16_t p = px[i];
            uint8_t rgb[3] = {(uint8_t)((p >> 11) * 255 / 31), (uint8_t)(((p >> 5) & 63) * 255 / 63), (uint8_t)((p & 31) * 255 / 31)};
            fwrite(rgb, 1, 3, f);
        }
        fclose(f);
    }
}
/* Script de touches : chaque entrée {image_début, bouton} est tenue 6 images. */
static uint8_t buttons(void *u) {
    T *t = (T *)u;
    uint8_t b = 0;
    for (int i = 0; i < t->script_n; i++)
        if (t->frame >= t->script[i][0] && t->frame < (uint32_t)t->script[i][0] + 6) b |= t->script[i][1];
    return b;
}
/* Compte les échantillons et relève le pic d'amplitude (pic=0 : silence). */
static void audio(const int16_t *s, size_t n, void *u) {
    T *t = (T *)u;
    t->audio_frames += n;
    for (size_t i = 0; i < n * 2; i++) { int16_t v = s[i] < 0 ? -s[i] : s[i]; if (v > t->audio_peak) t->audio_peak = v; }
}
static uint64_t now_us(void *u) { (void)u; struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return (uint64_t)ts.tv_sec * 1000000u + ts.tv_nsec / 1000; }
static void sleep_us(uint32_t us, void *u) { (void)us; (void)u; /* pas de pacing : on mesure la vitesse brute */ }

/* usage : host_test rom.gb frames [dump_frame out.ppm] [save_dir]. Appuie Start puis A en alternance pour passer les écrans titre. */
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s rom.gb frames [dump_frame out.ppm] [save_dir]\n", argv[0]); return 2; }
    static T t;
    memset(&t, 0, sizeof t);
    uint32_t frames = (uint32_t)atoi(argv[2]);
    if (argc >= 5) { t.dump_frame = (uint32_t)atoi(argv[3]); t.ppm = argv[4]; }
    const char *save_dir = argc >= 6 ? argv[5] : NULL;
    /* Appuis Start puis A espacés pour passer l'écran titre. */
    uint8_t seq[] = {GBRT_AKA_BTN_START, GBRT_AKA_BTN_A};
    for (int i = 0; i < 12; i++) { t.script[i][0] = (uint8_t)(120 + i * 20); t.script[i][1] = seq[i & 1]; }
    t.script_n = 12;
    GbrtAkaHal hal = {present, buttons, audio, now_us, sleep_us, NULL, &t};
    uint64_t t0 = now_us(NULL);
    int rc = gbrt_aka_run(&hal, argv[1], save_dir, frames);
    double sec = (now_us(NULL) - t0) / 1e6;
    printf("rc=%d frames=%u  %.2fs  %.1f fps (PC, interpreteur)  audio=%llu ech. pic=%d\n",
           rc, t.frame, sec, t.frame / sec, (unsigned long long)t.audio_frames, t.audio_peak);
    return rc;
}
