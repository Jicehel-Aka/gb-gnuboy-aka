/**
 * @file gbrt_browser.c
 * @brief Implémentation du navigateur de dossiers de ROM (lecture, tri, navigation, sélection).
 *
 * Fonctions : gbrt_browser_open/close/rescan (cycle de vie), gbrt_browser_move (curseur et défilement),
 * gbrt_browser_activate (ouvrir un dossier ou choisir une ROM), gbrt_browser_up (dossier parent), gbrt_browser_rel (chemin affichable).
 * Testée par tests/test_browser.c (ASAN/UBSAN).
 *
 * Projet  : gb-gnuboy-aka — lecteur Game Boy / Game Boy Color pour la Gamebuino AKA (ESP32-S3).
 * Auteur  : Jicehel (Jicehel-Aka)
 * Licence : voir README.md (cœur gnuboy : GPL v2 (components/gnuboy/COPYING) ; composant gamebuino : LGPL).
 */
#include "gbrt_browser.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* Minuscule ASCII (indépendante de la locale : tri identique sur l'AKA et sur PC). */
static int lower(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }

/* Comparaison de noms sans tenir compte de la casse (ordre alphabétique naturel pour l'utilisateur). */
static int name_cmp(const char *a, const char *b) {
    for (;; ++a, ++b) {
        int ca = lower((unsigned char)*a), cb = lower((unsigned char)*b);
        if (ca != cb) return ca - cb;
        if (!ca) return 0;
    }
}

/* Ordre d'affichage : dossiers d'abord, puis alphabétique. Comparateur pour qsort(). */
static int entry_cmp(const void *pa, const void *pb) {
    const GbrtEntry *a = (const GbrtEntry *)pa, *b = (const GbrtEntry *)pb;
    if (a->is_dir != b->is_dir) return a->is_dir ? -1 : 1;
    return name_cmp(a->name, b->name);
}

/* Vrai si le nom se termine par .gb, .gbc ou .zip (quelle que soit la casse ; un .zip est supposé contenir une ROM). */
static bool has_rom_ext(const char *name) {
    const char *dot = strrchr(name, '.');
    return dot && (name_cmp(dot, ".gb") == 0 || name_cmp(dot, ".gbc") == 0 || name_cmp(dot, ".zip") == 0);
}

/* dir + "/" + name dans out, sans doubler le séparateur ; false si ça ne tient pas dans `cap`. */
static bool join_path(char *out, size_t cap, const char *dir, const char *name) {
    size_t n = strlen(dir);
    int w = snprintf(out, cap, "%s%s%s", dir, (n && (dir[n - 1] == '/' || dir[n - 1] == '\\')) ? "" : "/", name);
    return w > 0 && (size_t)w < cap;
}

/* Vrai si path est un dossier (stat, car d_type n'est pas fiable sur FAT ni MinGW). */
static bool path_is_dir(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

/* Relit le dossier courant : ignore fichiers cachés (.xxx), noms trop longs et fichiers non ROM ; trie ; replace la sélection
 * sur l'élément qui était sélectionné avant la relecture, s'il existe encore. */
bool gbrt_browser_rescan(GbrtBrowser *b) {
    if (!b) return false;
    DIR *d = opendir(b->cwd);
    if (!d) return false;

    char keep[GBRT_BROWSER_NAME_MAX] = {0};
    if (b->entries && b->sel >= 0 && b->sel < b->count) snprintf(keep, sizeof(keep), "%s", b->entries[b->sel].name);

    b->count = 0;
    b->truncated = false;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        const size_t nlen = strlen(e->d_name);
        if (nlen >= GBRT_BROWSER_NAME_MAX) continue; /* nom trop long : ignoré */
        char full[GBRT_BROWSER_PATH_MAX];
        if (!join_path(full, sizeof(full), b->cwd, e->d_name)) continue;
        /* d_type n'est pas fiable partout (FAT, MinGW) : on interroge le fichier. */
        bool dir = path_is_dir(full);
        if (!dir && !has_rom_ext(e->d_name)) continue;
        if (b->count >= GBRT_BROWSER_MAX_ENTRIES) { b->truncated = true; break; }
        GbrtEntry *dst = &b->entries[b->count++];
        memcpy(dst->name, e->d_name, nlen + 1);
        dst->is_dir = dir;
    }
    closedir(d);
    qsort(b->entries, (size_t)b->count, sizeof(GbrtEntry), entry_cmp);

    b->sel = 0;
    for (int i = 0; keep[0] && i < b->count; ++i)
        if (strcmp(b->entries[i].name, keep) == 0) { b->sel = i; break; }
    b->top = 0;
    return true;
}

/* Alloue la liste (1024 entrées, ~100 Ko, en PSRAM), fixe la racine (sans séparateur final) et lit le dossier. */
bool gbrt_browser_open(GbrtBrowser *b, const char *root) {
    if (!b || !root) return false;
    memset(b, 0, sizeof(*b));
    b->entries = (GbrtEntry *)calloc(GBRT_BROWSER_MAX_ENTRIES, sizeof(GbrtEntry));
    if (!b->entries) return false;
    snprintf(b->root, sizeof(b->root), "%s", root);
    size_t n = strlen(b->root);
    while (n > 1 && (b->root[n - 1] == '/' || b->root[n - 1] == '\\')) b->root[--n] = 0;
    snprintf(b->cwd, sizeof(b->cwd), "%s", b->root);
    if (!gbrt_browser_rescan(b)) {
        gbrt_browser_close(b);
        return false;
    }
    return true;
}

/* Libère la liste ; le navigateur peut être rouvert avec gbrt_browser_open(). */
void gbrt_browser_close(GbrtBrowser *b) {
    if (!b) return;
    free(b->entries);
    memset(b, 0, sizeof(*b));
}

/* Déplace la sélection de `delta` lignes avec retour circulaire, puis ajuste `top` pour qu'elle reste dans les `rows` lignes visibles. */
void gbrt_browser_move(GbrtBrowser *b, int delta, int rows) {
    if (!b || b->count <= 0) return;
    b->sel = ((b->sel + delta) % b->count + b->count) % b->count;
    if (rows < 1) rows = 1;
    if (b->sel < b->top) b->top = b->sel;
    if (b->sel >= b->top + rows) b->top = b->sel - rows + 1;
    if (b->top < 0) b->top = 0;
}

/* Valide la sélection : dossier -> on y entre (0) ; fichier -> chemin complet dans `out` (1) ; vide ou erreur -> -1. */
int gbrt_browser_activate(GbrtBrowser *b, char *out, size_t cap) {
    if (!b || b->count <= 0) return -1;
    const GbrtEntry *e = &b->entries[b->sel];
    char full[GBRT_BROWSER_PATH_MAX];
    if (!join_path(full, sizeof(full), b->cwd, e->name)) return -1;
    if (e->is_dir) {
        char prev[GBRT_BROWSER_PATH_MAX];
        snprintf(prev, sizeof(prev), "%s", b->cwd);
        snprintf(b->cwd, sizeof(b->cwd), "%s", full);
        b->count = 0; /* on ne garde pas la sélection du dossier précédent */
        if (!gbrt_browser_rescan(b)) { /* dossier illisible : on reste où on est */
            snprintf(b->cwd, sizeof(b->cwd), "%s", prev);
            gbrt_browser_rescan(b);
            return -1;
        }
        return 0;
    }
    if (!out || (size_t)snprintf(out, cap, "%s", full) >= cap) return -1;
    return 1;
}

/* Remonte au dossier parent (jamais au-dessus de la racine) et sélectionne le dossier qu'on vient de quitter. */
bool gbrt_browser_up(GbrtBrowser *b) {
    if (!b || strcmp(b->cwd, b->root) == 0) return false;
    char child[GBRT_BROWSER_NAME_MAX];
    const char *slash = strrchr(b->cwd, '/');
    const char *bslash = strrchr(b->cwd, '\\');
    if (bslash && (!slash || bslash > slash)) slash = bslash;
    if (!slash) return false;
    snprintf(child, sizeof(child), "%s", slash + 1);
    b->cwd[slash - b->cwd] = 0;
    b->count = 0;
    if (strlen(b->cwd) < strlen(b->root)) snprintf(b->cwd, sizeof(b->cwd), "%s", b->root);
    /* Remet la sélection sur le dossier qu'on vient de quitter. */
    bool ok = gbrt_browser_rescan(b);
    for (int i = 0; ok && i < b->count; ++i)
        if (b->entries[i].is_dir && strcmp(b->entries[i].name, child) == 0) { b->sel = i; break; }
    return ok;
}

/* Chemin courant relatif à la racine, pour l'affichage ("/" à la racine). */
const char *gbrt_browser_rel(const GbrtBrowser *b) {
    if (!b) return "/";
    size_t rl = strlen(b->root);
    if (strncmp(b->cwd, b->root, rl) != 0 || b->cwd[rl] == 0) return "/";
    return b->cwd + rl;
}
