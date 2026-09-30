/**
 * @file gbrt_aka.c
 * @brief Couche plateforme AKA : fait tourner une ROM Game Boy / Game Boy Color sur le cœur gnuboy.
 *
 * @details Ce fichier est le « pont » entre gnuboy (CPU, LCD, son, mappers ; components/gnuboy) et l'application
 * hôte (lanceur AKA ou frontend PC). Il est portable C11 : il ne dépend que de la libc, de gnuboy et des fonctions
 * que l'hôte lui passe dans la structure GbrtAkaHal (affichage, boutons, son, horloge). L'interface (gbrt_aka.h) est
 * la même que dans la version « gnuboy » du lecteur : lanceur, navigateur, zip et frontend PC sont inchangés.
 *
 * Contenu :
 *  - conversions d'image RGB555 / RGBA -> RGB565 / BGR565 (format de l'écran AKA) et agrandissement 1,5x ;
 *  - chargement de la ROM (fichier ou .zip), complétée si l'en-tête annonce plus que le fichier ;
 *  - sauvegarde batterie + horloge dans <save_dir>/<nom>.sav (format gnuboy/VBA-M), écrite de façon atomique
 *    (fichier .tmp puis renommage) après 2 s sans nouvelle écriture de la ROM, et à la fermeture ;
 *  - boucle principale gbrt_aka_run() cadencée à 59,73 Hz avec saut d'affichage adaptatif (gnuboy_run(draw = false)
 *    saute vraiment le calcul des lignes, pas seulement le transfert écran).
 *
 * Projet  : gb-gnuboy-aka — lecteur Game Boy / Game Boy Color pour la Gamebuino AKA (ESP32-S3).
 * Auteur  : Jicehel (Jicehel-Aka)
 * Licence : GPL v2 (cœur gnuboy, voir components/gnuboy/COPYING et README.md). Composant gamebuino : LGPL.
 */
#include "gbrt_aka.h"
#include "gbrt_zip.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gnuboy.h"
#include "hw.h" /* cart.has_battery (état global de gnuboy) */

#define GB_CPU_HZ 4194304u      /* horloge du CPU Game Boy */
#define GB_FRAME_CYCLES 70224u  /* 154 lignes x 456 cycles : durée d'une image */
/* 16 742 us par frame Game Boy (soit 59,73 images/s). */
#define FRAME_US ((uint32_t)((uint64_t)GB_FRAME_CYCLES * 1000000u / GB_CPU_HZ))
#define ROM_MAX_BYTES (8u * 1024u * 1024u) /* plus grosse ROM Game Boy existante : 8 Mo */
#define AUDIO_BUF_SAMPLES 2048             /* int16 (1024 paires L/R) : une image en fait ~1 476 (44 100 Hz / 59,73) */
#define SAVE_DELAY_FRAMES 120              /* écriture de la sauvegarde 2 s après la dernière modification */
/* Palette des jeux Game Boy monochromes. GB_PALETTE_DMG = vert/gris de la console d'origine ; essayer
 * GB_PALETTE_CGB pour la colorisation automatique d'une Game Boy Color (palette choisie d'après le titre). */
#define DMG_PALETTE GB_PALETTE_DMG

/* État de la session en cours (une seule ROM à la fois : gnuboy est lui-même un singleton global). */
typedef struct AkaState {
    const GbrtAkaHal *hal;   /* fonctions fournies par l'hôte */
    bool frame_ready;        /* gnuboy a terminé une image visible pendant gnuboy_run */
} AkaState;

static AkaState g_state;
static char g_live_save_path[300]; /* chemin du .sav du jeu en cours (vide si aucun) : utilisé par gbrt_aka_flush_save() */


/* ------------------------------------------------------------------ */
/* Conversions d'image                                                 */
/* ------------------------------------------------------------------ */

/* RGBA 0xAARRGGBB -> RGB565 : on garde les 5/6/5 bits de poids fort de chaque canal. */
void gbrt_aka_rgba_to_rgb565(const uint32_t *src, uint16_t *dst, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        uint32_t p = src[i]; /* 0xAARRGGBB */
        dst[i] = (uint16_t)(((p >> 8) & 0xF800u) | ((p >> 5) & 0x07E0u) | ((p >> 3) & 0x001Fu));
    }
}

/* Tables 5 bits -> canal 8 bits (formule du runtime : v*255/31), puis réduction 565.
 * Construites une fois : l'exactitude vient de la formule, pas d'une approximation. */
static uint8_t s_r5[32], s_g6[32]; /* canal 5 bits -> 5 bits (rouge/bleu) ou 6 bits (vert) du format 565 */
static bool s_lut_ready;
static void lut_init(void) {
    for (int v = 0; v < 32; ++v) {
        const unsigned c8 = (unsigned)(v * 255 / 31);
        s_r5[v] = (uint8_t)(c8 >> 3);
        s_g6[v] = (uint8_t)(c8 >> 2);
    }
    s_lut_ready = true;
}

/* RGB555 (r = bits 0-4, v = 5-9, b = 10-14) -> BGR565 de l'écran AKA (bleu dans les bits hauts). Chemin le plus rapide. */
void gbrt_aka_rgb555_to_bgr565(const uint16_t *src, uint16_t *dst, size_t count) {
    if (!s_lut_ready) lut_init();
    for (size_t i = 0; i < count; ++i) {
        const uint32_t p = src[i];
        dst[i] = (uint16_t)(s_r5[p & 31u] | (s_g6[(p >> 5) & 31u] << 5) | (s_r5[(p >> 10) & 31u] << 11));
    }
}

/* RGB555 -> RGB565 classique (utile aux hôtes PC ou à d'autres écrans). */
void gbrt_aka_rgb555_to_rgb565(const uint16_t *src, uint16_t *dst, size_t count) {
    if (!s_lut_ready) lut_init();
    for (size_t i = 0; i < count; ++i) {
        const uint32_t p = src[i];
        dst[i] = (uint16_t)((s_r5[p & 31u] << 11) | (s_g6[(p >> 5) & 31u] << 5) | s_r5[(p >> 10) & 31u]);
    }
}

/* RGBA 0xAARRGGBB -> BGR565 : même résultat que lcd_color_rgb() du composant gamebuino. */
void gbrt_aka_rgba_to_bgr565(const uint32_t *src, uint16_t *dst, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        uint32_t p = src[i]; /* 0xAARRGGBB */
        dst[i] = (uint16_t)(((p >> 19) & 0x001Fu) | ((p >> 5) & 0x07E0u) | ((p << 8) & 0xF800u));
    }
}

/* Agrandissement 1,5x au plus proche voisin : chaque pixel de sortie (x, y) copie le pixel source (x*2/3, y*2/3). */
void gbrt_aka_scale_1_5x(const uint16_t *src, uint16_t *dst) {
    const int dw = GBRT_AKA_SCREEN_W * 3 / 2;
    const int dh = GBRT_AKA_SCREEN_H * 3 / 2;
    for (int y = 0; y < dh; ++y) {
        const uint16_t *row = src + (size_t)(y * 2 / 3) * GBRT_AKA_SCREEN_W;
        uint16_t *out = dst + (size_t)y * dw;
        for (int x = 0; x < dw; ++x) out[x] = row[x * 2 / 3];
    }
}

/* ------------------------------------------------------------------ */
/* Callbacks de gnuboy                                                 */
/* ------------------------------------------------------------------ */

/* Appelé par gnuboy au début du vblank : l'image est complète dans le tampon fourni. */
static void aka_video_cb(void *buffer) {
    (void)buffer;
    g_state.frame_ready = true;
}

/* Appelé par gnuboy avec `length` échantillons int16 stéréo entrelacés (fin d'image, ou tampon plein en cours d'image). */
static void aka_audio_cb(void *buffer, size_t length) {
    const GbrtAkaHal *hal = g_state.hal;
    if (hal && hal->audio_write && length >= 2) hal->audio_write((const int16_t *)buffer, length / 2, hal->user);
}

/* ------------------------------------------------------------------ */
/* Sauvegarde batterie + horloge                                       */
/* ------------------------------------------------------------------ */

/* Écrit la sauvegarde (tous les bancs de RAM + horloge) dans "<path>.tmp" puis remplace le fichier final : une coupure
 * de courant en pleine écriture laisse l'ancienne sauvegarde intacte (le renommage ne remplace pas un fichier existant
 * sur FAT, d'où le remove()). Renvoie true si le fichier final est en place. */
static bool save_sram_atomic(const char *path) {
    char tmp[300];
    if (snprintf(tmp, sizeof(tmp), "%s.tmp", path) >= (int)sizeof(tmp)) return false;
    if (gnuboy_save_sram(tmp, false) != 0) { remove(tmp); return false; }
    remove(path); /* rename() n'écrase pas sur FAT */
    if (rename(tmp, path) != 0) { remove(tmp); return false; }
    return true;
}

/* Écrit tout de suite la RAM batterie du jeu en cours (appelé avant un retour au loader, qui redémarre la console). */
void gbrt_aka_flush_save(void) {
    if (g_live_save_path[0]) save_sram_atomic(g_live_save_path);
}

/* ------------------------------------------------------------------ */
/* Chargement de la ROM                                                */
/* ------------------------------------------------------------------ */

/* Message lisible pour un code d'erreur (affiché par le lanceur). */
const char *gbrt_aka_strerror(int rc) {
    switch (rc) {
    case GBRT_AKA_OK: return "OK";
    case GBRT_AKA_ERR_ARGS: return "Arguments invalides";
    case GBRT_AKA_ERR_ROM_OPEN: return "Fichier illisible";
    case GBRT_AKA_ERR_ROM_SIZE: return "Taille de ROM invalide";
    case GBRT_AKA_ERR_NOMEM: return "Memoire insuffisante";
    case GBRT_AKA_ERR_CONTEXT: return "Emulateur non initialise";
    case GBRT_AKA_ERR_ZIP: return "Zip non gere ou corrompu";
    case GBRT_AKA_ERR_ZIP_NO_ROM: return "Aucune ROM .gb/.gbc dans le zip";
    default: return "Erreur inconnue";
    }
}

/* Lit une ROM d'un .zip (première entrée .gb/.gbc) et traduit le code d'erreur du module zip. */
static int read_rom_zip(const char *path, uint8_t **out, size_t *out_size) {
    int r = gbrt_zip_read_rom(path, out, out_size, ROM_MAX_BYTES, NULL, 0);
    switch (r) {
    case GBRT_ZIP_OK: return GBRT_AKA_OK;
    case GBRT_ZIP_ERR_IO: return GBRT_AKA_ERR_ROM_OPEN;
    case GBRT_ZIP_ERR_NO_ROM: return GBRT_AKA_ERR_ZIP_NO_ROM;
    case GBRT_ZIP_ERR_SIZE: return GBRT_AKA_ERR_ROM_SIZE;
    case GBRT_ZIP_ERR_NOMEM: return GBRT_AKA_ERR_NOMEM;
    default: return GBRT_AKA_ERR_ZIP;
    }
}

/* Lit toute la ROM en mémoire (malloc). Refuse < 0x150 octets (pas d'en-tête complet) ou > 8 Mo.
 * Un fichier .zip est décompressé à la volée (voir gbrt_zip.h). */
static int read_rom(const char *path, uint8_t **out, size_t *out_size) {
    if (gbrt_zip_has_ext(path)) return read_rom_zip(path, out, out_size);
    FILE *f = fopen(path, "rb");
    if (!f) return GBRT_AKA_ERR_ROM_OPEN;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return GBRT_AKA_ERR_ROM_OPEN; }
    long sz = ftell(f);
    if (sz < 0x150 || (size_t)sz > ROM_MAX_BYTES) { fclose(f); return GBRT_AKA_ERR_ROM_SIZE; }
    rewind(f);
    uint8_t *buf = (uint8_t *)malloc((size_t)sz);
    if (!buf) { fclose(f); return GBRT_AKA_ERR_NOMEM; }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) { free(buf); fclose(f); return GBRT_AKA_ERR_ROM_OPEN; }
    fclose(f);
    *out = buf;
    *out_size = (size_t)sz;
    return GBRT_AKA_OK;
}

/* Identifiant de sauvegarde = nom du fichier ROM sans dossier ni extension (la DERNIÈRE seulement :
 * "super.rc.pro.am.gb" -> "super.rc.pro.am", et non "super"). Le contexte du runtime limite l'identifiant à 63 caractères :
 * un nom plus long est raccourci puis complété par un hachage FNV-1a du nom complet, pour que deux
 * ROM aux noms longs et presque identiques (ex. variantes régionales) gardent des sauvegardes distinctes. */
void gbrt_aka_save_id_from_path(const char *path, char *out, size_t cap) {
    if (!out || cap == 0) return;
    out[0] = 0;
    if (!path) return;
    const char *base = path;
    for (const char *p = path; *p; ++p)
        if (*p == '/' || *p == '\\') base = p + 1;
    size_t len = strlen(base);
    const char *dot = strrchr(base, '.');
    if (dot && dot != base) len = (size_t)(dot - base);

    if (len + 1 <= cap) { /* tient tel quel */
        memcpy(out, base, len);
        out[len] = 0;
        return;
    }
    /* Trop long : préfixe + '_' + 7 chiffres hexa du hachage (nécessite cap >= 10). */
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < len; ++i) { h ^= (uint8_t)base[i]; h *= 16777619u; }
    if (cap < 10) { memcpy(out, base, cap - 1); out[cap - 1] = 0; return; }
    const size_t keep = cap - 9;
    memcpy(out, base, keep);
    snprintf(out + keep, 9, "_%07x", (unsigned)(h & 0x0FFFFFFFu));
}

/* ------------------------------------------------------------------ */
/* Boucle principale                                                   */
/* ------------------------------------------------------------------ */

/* Taille en octets annoncée par l'en-tête (octet 0x148), 0 si le code est inconnu (gnuboy refuse la ROM). */
static size_t declared_rom_bytes(const uint8_t *rom) {
    const unsigned c = rom[0x148];
    if (c < 9) return (size_t)0x8000u << c;
    if (c > 0x51 && c < 0x55) return (size_t)128 * 0x4000u; /* codes 0x52-0x54 : gnuboy les traite comme 128 bancs */
    return 0;
}

/* Affiche l'image via le chemin le plus direct proposé par l'hôte. `fb` = RGB555 gnuboy (bits 0-4 rouge). */
static void present_frame(const GbrtAkaHal *hal, const uint16_t *fb, uint16_t *scratch16, uint32_t *scratch32) {
    if (hal->present_rgb555) {
        hal->present_rgb555(fb, GBRT_AKA_SCREEN_W, GBRT_AKA_SCREEN_H, hal->user);
    } else if (hal->present_rgba) {
        for (size_t i = 0; i < (size_t)GBRT_AKA_SCREEN_W * GBRT_AKA_SCREEN_H; ++i) {
            const uint32_t p = fb[i];
            const uint32_t r = (p & 31u) * 255u / 31u, g = ((p >> 5) & 31u) * 255u / 31u, b = ((p >> 10) & 31u) * 255u / 31u;
            scratch32[i] = 0xFF000000u | (r << 16) | (g << 8) | b;
        }
        hal->present_rgba(scratch32, GBRT_AKA_SCREEN_W, GBRT_AKA_SCREEN_H, hal->user);
    } else {
        const size_t n = (size_t)GBRT_AKA_SCREEN_W * GBRT_AKA_SCREEN_H;
        if (hal->pixel_bgr565) gbrt_aka_rgb555_to_bgr565(fb, scratch16, n);
        else gbrt_aka_rgb555_to_rgb565(fb, scratch16, n);
        hal->present(scratch16, GBRT_AKA_SCREEN_W, GBRT_AKA_SCREEN_H, hal->user);
    }
}

/* Déroulement : valider les arguments -> lire la ROM -> initialiser gnuboy -> charger la sauvegarde -> boucle d'images
 * (entrées, émulation d'une image, affichage, cadence) -> écrire la sauvegarde -> libérer gnuboy. */
int gbrt_aka_run(const GbrtAkaHal *hal, const char *rom_path,
                 const char *save_dir, uint32_t max_frames) {
    if (!hal || !(hal->present || hal->present_rgba || hal->present_rgb555) || !hal->read_buttons || !hal->time_us ||
        !hal->sleep_us || !rom_path)
        return GBRT_AKA_ERR_ARGS;

    uint8_t *rom = NULL;
    size_t rom_size = 0;
    int rc = read_rom(rom_path, &rom, &rom_size);
    if (rc != GBRT_AKA_OK) return rc;

    /* gnuboy lit les bancs directement dans ce tampon (pas de copie) : il doit couvrir toute la taille annoncée. */
    const size_t want = declared_rom_bytes(rom);
    if (want == 0) { free(rom); return GBRT_AKA_ERR_ROM_SIZE; }
    if (rom_size < want) {
        uint8_t *bigger = (uint8_t *)realloc(rom, want);
        if (!bigger) { free(rom); return GBRT_AKA_ERR_NOMEM; }
        memset(bigger + rom_size, 0xFF, want - rom_size); /* comme une cartouche dont la ROM est tronquée */
        rom = bigger;
        rom_size = want;
    }

    memset(&g_state, 0, sizeof(g_state));
    g_state.hal = hal;

    uint16_t *fb = (uint16_t *)gb_calloc_fast((size_t)GBRT_AKA_SCREEN_W * GBRT_AKA_SCREEN_H, sizeof(uint16_t));
    int16_t *abuf = hal->audio_write ? (int16_t *)gb_calloc_fast(AUDIO_BUF_SAMPLES, sizeof(int16_t)) : NULL;
    uint16_t *scratch16 = NULL;
    uint32_t *scratch32 = NULL;
    if (!hal->present_rgb555) {
        if (hal->present_rgba) scratch32 = (uint32_t *)malloc(sizeof(uint32_t) * GBRT_AKA_SCREEN_W * GBRT_AKA_SCREEN_H);
        else scratch16 = (uint16_t *)malloc(sizeof(uint16_t) * GBRT_AKA_SCREEN_W * GBRT_AKA_SCREEN_H);
    }
    if (!fb || (hal->audio_write && !abuf) ||
        (!hal->present_rgb555 && !scratch16 && !scratch32)) {
        free(fb); free(abuf); free(scratch16); free(scratch32); free(rom);
        return GBRT_AKA_ERR_NOMEM;
    }

    if (gnuboy_init(GBRT_AKA_AUDIO_RATE, GB_AUDIO_STEREO_S16, GB_PIXEL_555, aka_video_cb, aka_audio_cb) < 0) {
        free(fb); free(abuf); free(scratch16); free(scratch32); free(rom);
        return GBRT_AKA_ERR_CONTEXT;
    }
    gnuboy_set_framebuffer(fb);
    gnuboy_set_soundbuffer(abuf, abuf ? AUDIO_BUF_SAMPLES : 0);
    if (gnuboy_load_rom(rom, rom_size) != 0) {
        gnuboy_free();
        free(fb); free(abuf); free(scratch16); free(scratch32); free(rom);
        return GBRT_AKA_ERR_ROM_SIZE;
    }
    if (gnuboy_get_hwtype() != GB_HW_CGB) gnuboy_set_palette(DMG_PALETTE);
    gnuboy_reset(true);

    /* Sauvegarde : <save_dir>/<nom>.sav ; chargée si elle existe. */
    char save_path[300] = "";
    if (save_dir && save_dir[0] && cart.has_battery) {
        char id[64];
        gbrt_aka_save_id_from_path(rom_path, id, sizeof(id));
        if (snprintf(save_path, sizeof(save_path), "%s/%s.sav", save_dir, id) >= (int)sizeof(save_path)) save_path[0] = 0;
        else gnuboy_load_sram(save_path);
    }
    snprintf(g_live_save_path, sizeof(g_live_save_path), "%s", save_path);
    uint32_t dirty_frames = 0; /* images écoulées depuis la dernière écriture de la RAM cartouche (0 = rien à écrire) */

    uint64_t next_deadline = hal->time_us(hal->user);
    uint32_t frames = 0;
    /* Mesure de charge et saut d'affichage adaptatif. */
    uint64_t rep_t0 = next_deadline, busy_us = 0;
    uint32_t rep_frames = 0, skipped_total = 0, skip_streak = 0;
    bool skip_present = false;
    for (;;) {
        if (hal->should_quit && hal->should_quit(hal->user)) break;
        if (max_frames && frames >= max_frames) break;

        const uint64_t frame_t0 = hal->time_us(hal->user);
        gnuboy_set_pad(hal->read_buttons(hal->user)); /* mêmes bits que GBRT_AKA_BTN_* */

        g_state.frame_ready = false;
        gnuboy_run(!skip_present);

        if (skip_present) {
            ++skipped_total;
        } else {
            if (!g_state.frame_ready) /* LCD éteint : gnuboy n'affiche rien, l'écran réel serait blanc */
                for (size_t i = 0; i < (size_t)GBRT_AKA_SCREEN_W * GBRT_AKA_SCREEN_H; ++i) fb[i] = 0x7FFF;
            present_frame(hal, fb, scratch16, scratch32);
        }
        ++frames;

        /* Sauvegarde différée : on attend 2 s sans nouvelle écriture pour regrouper les enregistrements. */
        if (save_path[0]) {
            if (gnuboy_sram_dirty()) {
                if (++dirty_frames >= SAVE_DELAY_FRAMES) {
                    if (save_sram_atomic(save_path)) dirty_frames = 0;
                    else dirty_frames = SAVE_DELAY_FRAMES / 2; /* échec : on réessaie dans 1 s */
                }
            } else dirty_frames = 0;
        }

        /* Cadence 59,73 Hz. En cas de retard, on ne rattrape pas au-delà de 2 frames. */
        next_deadline += FRAME_US;
        uint64_t now = hal->time_us(hal->user);
        busy_us += now - frame_t0;
        const bool late = now > next_deadline;
        if (hal->auto_frameskip && late && skip_streak < 2) { skip_present = true; ++skip_streak; }
        else { skip_present = false; skip_streak = 0; }
        if (++rep_frames == 120) {
            if (hal->report) {
                const double wall = (double)(now - rep_t0);
                hal->report(wall > 0 ? (float)(rep_frames * 1e6 / wall) : 0.0f,
                            (int)(busy_us * 100 / ((uint64_t)FRAME_US * rep_frames)), skipped_total, hal->user);
            }
            rep_t0 = now; busy_us = 0; rep_frames = 0;
        }
        if (next_deadline > now)
            hal->sleep_us((uint32_t)(next_deadline - now), hal->user);
        else if (now - next_deadline > 2u * FRAME_US)
            next_deadline = now;
    }

    if (save_path[0] && gnuboy_sram_dirty()) save_sram_atomic(save_path); /* dernière écriture à la fermeture */
    g_live_save_path[0] = 0;
    gnuboy_free();
    free(fb); free(abuf); free(scratch16); free(scratch32); free(rom);
    return GBRT_AKA_OK;
}
