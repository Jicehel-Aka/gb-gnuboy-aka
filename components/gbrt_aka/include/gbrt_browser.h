/**
 * @file gbrt_browser.h
 * @brief Navigateur de dossiers de ROM Game Boy (logique seule, sans affichage).
 *
 * Partagé entre le lanceur AKA et le lanceur PC : chacun dessine la liste à sa façon.
 * Les dossiers passent avant les fichiers ; seuls .gb et .gbc sont listés (insensible à la casse).
 * Le navigateur ne sort jamais du dossier racine. Jusqu'à 1024 entrées par dossier ; les noms de 96 caractères ou plus sont ignorés.
 *
 * Projet  : gb-gnuboy-aka — lecteur Game Boy / Game Boy Color pour la Gamebuino AKA (ESP32-S3).
 * Auteur  : Jicehel (Jicehel-Aka)
 * Licence : voir README.md (cœur gnuboy : GPL v2 (components/gnuboy/COPYING) ; composant gamebuino : LGPL).
 */
#ifndef GBRT_BROWSER_H
#define GBRT_BROWSER_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GBRT_BROWSER_NAME_MAX 96
#define GBRT_BROWSER_PATH_MAX 300
#define GBRT_BROWSER_MAX_ENTRIES 1024

typedef struct GbrtEntry {
    char name[GBRT_BROWSER_NAME_MAX];
    bool is_dir;
} GbrtEntry;

typedef struct GbrtBrowser {
    char root[GBRT_BROWSER_PATH_MAX];
    char cwd[GBRT_BROWSER_PATH_MAX];
    GbrtEntry *entries;
    int count;
    int sel;
    int top;
    bool truncated; /* plus de GBRT_BROWSER_MAX_ENTRIES éléments : la liste est coupée */
} GbrtBrowser;

/** Ouvre le navigateur sur `root` et lit le dossier. Renvoie false si root est illisible. */
bool gbrt_browser_open(GbrtBrowser *b, const char *root);
void gbrt_browser_close(GbrtBrowser *b);

/** Relit le dossier courant (garde la sélection si possible). */
bool gbrt_browser_rescan(GbrtBrowser *b);

/** Déplace la sélection (avec retour à l'autre bout) et garde `rows` lignes visibles. */
void gbrt_browser_move(GbrtBrowser *b, int delta, int rows);

/**
 * Valide l'élément sélectionné.
 * Dossier : y entre, renvoie 0. Fichier : écrit son chemin complet dans out et renvoie 1.
 * Liste vide : renvoie -1.
 */
int gbrt_browser_activate(GbrtBrowser *b, char *out, size_t cap);

/** Remonte d'un dossier, sans dépasser la racine. Renvoie false si on est déjà à la racine. */
bool gbrt_browser_up(GbrtBrowser *b);

/** Chemin du dossier courant relatif à la racine ("/" à la racine, "/JEUX/GB" sinon). */
const char *gbrt_browser_rel(const GbrtBrowser *b);

#ifdef __cplusplus
}
#endif

#endif /* GBRT_BROWSER_H */
