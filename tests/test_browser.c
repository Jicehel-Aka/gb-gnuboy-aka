/**
 * @file test_browser.c
 * @brief Test autonome du navigateur de dossiers : crée une arborescence temporaire et vérifie tri, filtre, navigation.
 *
 * Projet  : gb-gnuboy-aka — lecteur Game Boy / Game Boy Color pour la Gamebuino AKA (ESP32-S3).
 * Auteur  : Jicehel (Jicehel-Aka)
 * Licence : voir README.md (cœur gnuboy : GPL v2 (components/gnuboy/COPYING) ; composant gamebuino : LGPL).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "gbrt_browser.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("ECHEC ligne %d : %s\n", __LINE__, #c); fails++; } } while (0)

/* Crée un petit fichier (1 octet) : suffit pour le navigateur qui ne lit que les noms. */
static void touch(const char *root, const char *rel) {
    char p[400];
    snprintf(p, sizeof p, "%s/%s", root, rel);
    FILE *f = fopen(p, "wb");
    if (f) { fputc(0, f); fclose(f); }
}
/* Crée un sous-dossier (portable Windows/POSIX). */
static void mkd(const char *root, const char *rel) {
    char p[400];
    snprintf(p, sizeof p, "%s/%s", root, rel);
#ifdef _WIN32
    mkdir(p);
#else
    mkdir(p, 0777);
#endif
}

/* Arborescence : Action/(Shmups/Solar.gbc, racer.gb), Puzzle, vide, Alpha.GB, zeta.gb + fichiers à ignorer (.txt, .cache.gb). */
int main(void) {
    char root[] = "gbrt_browser_test_XXXXXX";
#ifdef _WIN32
    strcpy(root, "gbrt_browser_test_dir");
    mkdir(root);
#else
    if (!mkdtemp(root)) { perror("mkdtemp"); return 2; }
#endif
    mkd(root, "Action"); mkd(root, "Action/Shmups"); mkd(root, "Puzzle"); mkd(root, "vide");
    touch(root, "zeta.gb"); touch(root, "Alpha.GB"); touch(root, "Action/Shmups/Solar.gbc");
    touch(root, "Action/racer.gb"); touch(root, "notes.txt"); touch(root, ".cache.gb"); touch(root, "Puzzle/readme.md");

    GbrtBrowser b; char out[400];
    CHECK(!gbrt_browser_open(&b, "dossier_inexistant_xyz"));
    CHECK(gbrt_browser_open(&b, root));
    CHECK(b.count == 5); /* Action, Puzzle, vide, Alpha.GB, zeta.gb : ni .txt ni fichier caché */
    CHECK(b.entries[0].is_dir && !strcmp(b.entries[0].name, "Action"));
    CHECK(!b.entries[b.count - 1].is_dir && !strcmp(b.entries[b.count - 1].name, "zeta.gb"));
    CHECK(!strcmp(gbrt_browser_rel(&b), "/"));
    CHECK(!gbrt_browser_up(&b));
    gbrt_browser_move(&b, -1, 3); CHECK(b.sel == b.count - 1);
    gbrt_browser_move(&b, 1, 3);  CHECK(b.sel == 0);
    CHECK(gbrt_browser_activate(&b, out, sizeof out) == 0);
    CHECK(!strcmp(gbrt_browser_rel(&b), "/Action") && b.count == 2);
    CHECK(gbrt_browser_activate(&b, out, sizeof out) == 0);
    CHECK(!strcmp(gbrt_browser_rel(&b), "/Action/Shmups"));
    CHECK(gbrt_browser_activate(&b, out, sizeof out) == 1);
    CHECK(strstr(out, "Solar.gbc") != NULL);
    CHECK(gbrt_browser_up(&b) && !strcmp(b.entries[b.sel].name, "Shmups"));
    CHECK(gbrt_browser_up(&b) && !strcmp(b.entries[b.sel].name, "Action"));
    CHECK(!gbrt_browser_up(&b));
    gbrt_browser_move(&b, 2, 3); CHECK(!strcmp(b.entries[b.sel].name, "vide"));
    CHECK(gbrt_browser_activate(&b, out, sizeof out) == 0 && b.count == 0);
    CHECK(gbrt_browser_activate(&b, out, sizeof out) == -1);
    gbrt_browser_move(&b, 1, 3);
    CHECK(gbrt_browser_up(&b));
    gbrt_browser_close(&b);

    char cmd[600];
    snprintf(cmd, sizeof cmd, "rm -rf %s", root);
    if (system(cmd) != 0) { /* nettoyage au mieux */ }
    printf(fails ? "%d ECHEC(S)\n" : "navigateur : tous les tests passent\n", fails);
    return fails != 0;
}
