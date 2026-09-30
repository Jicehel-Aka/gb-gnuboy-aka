/*
 * test_saveid.c — tests de gbrt_aka_save_id_from_path().
 * Vérifie la dérivation du nom de sauvegarde : dossiers, points multiples, noms longs.
 */
#include <stdio.h>
#include <string.h>
#include "gbrt_aka.h"

static int fails;

static void expect_eq(const char *path, const char *want) {
    char id[64];
    gbrt_aka_save_id_from_path(path, id, sizeof(id));
    if (strcmp(id, want) != 0) { printf("ECHEC %-40s -> '%s' au lieu de '%s'\n", path, id, want); fails++; }
    else printf("ok    %-40s -> '%s'\n", path, id);
}

int main(void) {
    expect_eq("/sdcard/GB/Tetris.gb", "Tetris");
    expect_eq("C:\\roms\\Mario Land.gbc", "Mario Land");
    expect_eq("/sdcard/GB/super.rc.pro.am.gb", "super.rc.pro.am"); /* points multiples : seule la dernière extension saute */
    expect_eq("Zelda", "Zelda");                                   /* sans extension */
    expect_eq("/x/.cache", ".cache");                              /* fichier caché : pas de nom vide */

    /* Noms longs : longueur bornée, déterministe, et deux variantes distinctes restent distinctes. */
    char a[64], b[64], c[64];
    gbrt_aka_save_id_from_path("/GB/Pokemon - Yellow Version - Special Pikachu Edition (USA, Europe) (GBC,SGB Enhanced).gb", a, sizeof a);
    gbrt_aka_save_id_from_path("/GB/Pokemon - Yellow Version - Special Pikachu Edition (USA, Europe) (GBC,SGB Enhanced) (Rev 1).gb", b, sizeof b);
    gbrt_aka_save_id_from_path("/GB/Pokemon - Yellow Version - Special Pikachu Edition (USA, Europe) (GBC,SGB Enhanced).gb", c, sizeof c);
    if (strlen(a) != 63 || strlen(b) != 63) { printf("ECHEC longueur %zu/%zu (attendu 63)\n", strlen(a), strlen(b)); fails++; }
    else if (!strcmp(a, b)) { printf("ECHEC : deux noms longs différents donnent le même identifiant '%s'\n", a); fails++; }
    else if (strcmp(a, c)) { printf("ECHEC : identifiant non déterministe\n"); fails++; }
    else printf("ok    noms longs : '%s' / '%s'\n", a, b);

    printf(fails ? "%d ECHEC(S)\n" : "identifiants de sauvegarde : tous les tests passent\n", fails);
    return fails != 0;
}
