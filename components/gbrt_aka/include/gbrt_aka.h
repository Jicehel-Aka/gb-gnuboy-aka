/**
 * @file gbrt_aka.h
 * @brief Interface publique de la couche plateforme AKA (HAL, boutons, codes d'erreur, conversions d'image).
 *
 * Remplace platform_sdl.cpp du runtime amont. Le runtime tourne en mode interpréteur seul (gb_dispatch est un symbole
 * faible qui retombe sur gb_interpret) : la ROM est lue depuis un fichier (carte SD), aucun code recompilé n'est requis.
 * L'application fournit une structure GbrtAkaHal puis appelle gbrt_aka_run().
 *
 * Projet  : gb-gnuboy-aka — lecteur Game Boy / Game Boy Color pour la Gamebuino AKA (ESP32-S3).
 * Auteur  : Jicehel (Jicehel-Aka)
 * Licence : voir README.md (cœur gnuboy : GPL v2 (components/gnuboy/COPYING) ; composant gamebuino : LGPL).
 */
#ifndef GBRT_AKA_H
#define GBRT_AKA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GBRT_AKA_SCREEN_W 160
#define GBRT_AKA_SCREEN_H 144
#define GBRT_AKA_AUDIO_RATE 44100 /* imposé par audio.c du runtime */

/* Boutons renvoyés par read_buttons() (1 = appuyé). */
enum {
    GBRT_AKA_BTN_RIGHT  = 1u << 0,
    GBRT_AKA_BTN_LEFT   = 1u << 1,
    GBRT_AKA_BTN_UP     = 1u << 2,
    GBRT_AKA_BTN_DOWN   = 1u << 3,
    GBRT_AKA_BTN_A      = 1u << 4,
    GBRT_AKA_BTN_B      = 1u << 5,
    GBRT_AKA_BTN_SELECT = 1u << 6,
    GBRT_AKA_BTN_START  = 1u << 7,
};

enum {
    GBRT_AKA_OK = 0,
    GBRT_AKA_ERR_ARGS = -1,
    GBRT_AKA_ERR_ROM_OPEN = -2,
    GBRT_AKA_ERR_ROM_SIZE = -3,
    GBRT_AKA_ERR_NOMEM = -4,
    GBRT_AKA_ERR_CONTEXT = -5,
    GBRT_AKA_ERR_ZIP = -6,         /**< .zip illisible, non géré (ZIP64, chiffré, méthode rare) ou corrompu (CRC) */
    GBRT_AKA_ERR_ZIP_NO_ROM = -7,  /**< .zip sans entrée .gb / .gbc */
};

/** Message court (français, sans accent, <= 31 caractères) décrivant un code d'erreur de gbrt_aka_run. */
const char *gbrt_aka_strerror(int rc);

/** Écrit immédiatement la RAM batterie du jeu en cours (sans effet si aucun jeu ou pas de batterie).
 *  À appeler avant de redémarrer la console (retour au loader) ; peut l'être depuis un callback du HAL. */
void gbrt_aka_flush_save(void);

/**
 * Fonctions fournies par l'application. read_buttons, time_us et sleep_us sont obligatoires,
 * ainsi qu'un de : present, present_rgba ou present_rgb555.
 */
typedef struct GbrtAkaHal {
    /** Affiche une image 160x144, pixels 16 bits (RGB565, ou BGR565 si pixel_bgr565). */
    void (*present)(const uint16_t *pixels16, int w, int h, void *user);
    /** Masque de GBRT_AKA_BTN_* (1 = appuyé). */
    uint8_t (*read_buttons)(void *user);
    /** Échantillons stéréo entrelacés (L,R) à 44100 Hz, `frames` paires. Optionnel. */
    void (*audio_write)(const int16_t *stereo, size_t frames, void *user);
    uint64_t (*time_us)(void *user);
    void (*sleep_us)(uint32_t us, void *user);
    /** true pour quitter proprement (la RAM batterie est alors écrite). Optionnel. */
    bool (*should_quit)(void *user);
    void *user;
    /* --- Champs optionnels, ajoutés en fin de structure (une initialisation positionnelle ancienne reste valide) --- */
    /** Si présent, remplace present : reçoit directement l'image RGBA 0xAARRGGBB du PPU
     *  (160x144), sans tampon intermédiaire. Permet d'écrire tout droit dans le framebuffer de l'AKA. */
    void (*present_rgba)(const uint32_t *rgba, int w, int h, void *user);
    /** Chemin le plus rapide et le plus léger : reçoit l'image RGB555 du PPU (bits 0-4 rouge, 5-9 vert, 10-14 bleu),
     *  sans tampon RGBA (92 Ko de RAM et une conversion par image en moins). Convertir avec gbrt_aka_rgb555_to_bgr565().
     *  Prioritaire sur present_rgba et present. */
    void (*present_rgb555)(const uint16_t *rgb555, int w, int h, void *user);
    /** true : si l'émulation prend du retard, saute l'affichage de la prochaine image (2 de suite au plus).
     *  L'émulation et le son continuent normalement ; seul le coût de la conversion et du transfert écran est évité. */
    bool auto_frameskip;
    /** Optionnel : appelé toutes les 120 images avec la vitesse mesurée (images émulées/s), la charge
     *  (% du temps disponible par image réellement utilisé ; > 100 = trop lent) et le nombre d'affichages sautés. */
    void (*report)(float fps, int load_percent, uint32_t frames_skipped, void *user);
    /** true : present reçoit du BGR565 (bleu dans les bits hauts), format de l'écran AKA. */
    bool pixel_bgr565;
} GbrtAkaHal;

/**
 * Charge la ROM, exécute jusqu'à should_quit() (ou max_frames > 0), puis détruit
 * le contexte, ce qui écrit la RAM batterie. save_dir doit déjà exister ;
 * NULL désactive les sauvegardes. max_frames = 0 : sans limite.
 * Renvoie GBRT_AKA_OK ou un code d'erreur négatif.
 */
int gbrt_aka_run(const GbrtAkaHal *hal, const char *rom_path,
                 const char *save_dir, uint32_t max_frames);

/**
 * Identifiant de sauvegarde déduit du chemin de la ROM : nom de fichier sans dossier ni dernière extension,
 * raccourci avec un hachage s'il dépasse cap-1 caractères. Les fichiers sont /<save_dir>/<id>.sav et .rtc.
 */
void gbrt_aka_save_id_from_path(const char *path, char *out, size_t cap);

/** RGBA 32 bits du PPU -> RGB565. */
void gbrt_aka_rgba_to_rgb565(const uint32_t *src, uint16_t *dst, size_t count);

/** RGBA 32 bits du PPU -> BGR565 (identique à lcd_color_rgb() de l'AKA). */
void gbrt_aka_rgba_to_bgr565(const uint32_t *src, uint16_t *dst, size_t count);

/** RGB555 du PPU -> BGR565 (identique bit à bit à RGBA -> BGR565 via le chemin RGBA). */
void gbrt_aka_rgb555_to_bgr565(const uint16_t *src, uint16_t *dst, size_t count);
/** Idem vers RGB565. */
void gbrt_aka_rgb555_to_rgb565(const uint16_t *src, uint16_t *dst, size_t count);

/** Agrandissement 1,5x au plus proche voisin : 160x144 -> 240x216. */
void gbrt_aka_scale_1_5x(const uint16_t *src, uint16_t *dst);

#ifdef __cplusplus
}
#endif

#endif /* GBRT_AKA_H */
