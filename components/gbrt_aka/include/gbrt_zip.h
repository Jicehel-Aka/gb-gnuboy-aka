/**
 * @file gbrt_zip.h
 * @brief Lecture d'une ROM Game Boy stockée dans une archive .zip (deflate ou stocké).
 *
 * Choisit la première entrée .gb / .gbc de l'archive, la décompresse directement dans un tampon alloué
 * (taille connue par l'annuaire central du zip), puis vérifie son CRC-32. Le décodeur deflate est
 * interne (aucune dépendance : même code sur la console et sur PC, donc testable sur PC).
 *
 * Limites : pas de ZIP64, pas de chiffrement, méthodes 0 (stocké) et 8 (deflate) seulement.
 *
 * Projet  : gb-gnuboy-aka — lecteur Game Boy / Game Boy Color pour la Gamebuino AKA (ESP32-S3).
 * Auteur  : Jicehel (Jicehel-Aka)
 * Licence : voir README.md (cœur gnuboy : GPL v2 (components/gnuboy/COPYING) ; composant gamebuino : LGPL).
 */
#ifndef GBRT_ZIP_H
#define GBRT_ZIP_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Codes de retour de gbrt_zip_read_rom. */
typedef enum {
    GBRT_ZIP_OK = 0,
    GBRT_ZIP_ERR_IO = -1,      /**< fichier illisible / tronqué */
    GBRT_ZIP_ERR_FORMAT = -2,  /**< pas un zip, ZIP64, chiffré ou méthode non gérée */
    GBRT_ZIP_ERR_NO_ROM = -3,  /**< aucune entrée .gb / .gbc */
    GBRT_ZIP_ERR_SIZE = -4,    /**< ROM < 0x150 octets ou > max_size */
    GBRT_ZIP_ERR_NOMEM = -5,   /**< allocation impossible */
    GBRT_ZIP_ERR_CORRUPT = -6  /**< données deflate invalides ou CRC-32 incorrect */
} GbrtZipResult;

/**
 * Lit la première ROM (.gb/.gbc) du zip `path`.
 * @param out        reçoit un tampon malloc (à libérer avec free) contenant la ROM.
 * @param size       reçoit sa taille.
 * @param max_size   taille maximale acceptée de la ROM décompressée.
 * @param inner_name (optionnel) reçoit le nom de l'entrée choisie (tronqué à inner_cap-1).
 * @return GBRT_ZIP_OK ou un code GBRT_ZIP_ERR_*.
 */
int gbrt_zip_read_rom(const char *path, uint8_t **out, size_t *size, size_t max_size,
                      char *inner_name, size_t inner_cap);

/** Vrai si le nom de fichier se termine par ".zip" (casse ignorée). */
int gbrt_zip_has_ext(const char *name);

#ifdef __cplusplus
}
#endif
#endif /* GBRT_ZIP_H */
