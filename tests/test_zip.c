/*
 * test_zip.c — tests de gbrt_zip_read_rom() sur les archives fabriquées par tools/make_zip_fixtures.py.
 * usage : test_zip dossier_fixtures    (lit dossier/expected.txt : "<nom> <taille> <fnv1a>" ou "<nom> ERR <code>")
 * Vérifie le contenu exact (empreinte FNV-1a), les codes d'erreur et l'absence de fuite (ASAN).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gbrt_zip.h"

static uint32_t fnv(const uint8_t *p, size_t n) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 16777619u; }
    return h;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: test_zip dossier\n"); return 2; }
    char path[1024];
    snprintf(path, sizeof(path), "%s/expected.txt", argv[1]);
    FILE *ex = fopen(path, "r");
    if (!ex) { perror(path); return 2; }
    char name[128], a[64], b[64];
    int fails = 0, n = 0;
    while (fscanf(ex, "%127s %63s %63s", name, a, b) == 3) { /* chaque ligne : 3 mots ("nom ERR code" ou "nom taille hash") */
        snprintf(path, sizeof(path), "%s/%s.zip", argv[1], name);
        uint8_t *rom = NULL; size_t size = 0; char inner[64];
        int rc = gbrt_zip_read_rom(path, &rom, &size, 8u << 20, inner, sizeof(inner));
        ++n;
        if (strcmp(a, "ERR") == 0) {
            int want = atoi(b);
            if (rc != want) { printf("ECHEC %-24s rc=%d attendu %d\n", name, rc, want); ++fails; }
            else if (rom) { printf("ECHEC %-24s tampon non NULL en erreur\n", name); ++fails; }
            else printf("ok    %-24s erreur %d\n", name, rc);
        } else {
            size_t wsize = (size_t)strtoul(a, NULL, 10);
            uint32_t whash = (uint32_t)strtoul(b, NULL, 16);
            if (rc != 0 || size != wsize || fnv(rom, size) != whash) {
                printf("ECHEC %-24s rc=%d taille=%zu (attendu %zu)\n", name, rc, size, wsize); ++fails;
            } else printf("ok    %-24s %zu octets (%s)\n", name, size, inner);
        }
        free(rom);
    }
    fclose(ex);
    if (gbrt_zip_has_ext("A.ZIP") != 1 || gbrt_zip_has_ext("a.gb") != 0 || gbrt_zip_has_ext("zip") != 0 ||
        gbrt_zip_has_ext("x.zipx") != 0) { printf("ECHEC gbrt_zip_has_ext\n"); ++fails; }
    if (gbrt_zip_read_rom("/nonexistent/x.zip", &(uint8_t *){0}, &(size_t){0}, 1u << 20, NULL, 0) != GBRT_ZIP_ERR_IO) {
        printf("ECHEC fichier absent\n"); ++fails;
    }
    printf("%d cas, %d echec(s)\n", n, fails);
    return fails ? 1 : 0;
}
