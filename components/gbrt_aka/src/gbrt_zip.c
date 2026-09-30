/**
 * @file gbrt_zip.c
 * @brief Lecture d'une ROM dans un .zip : annuaire central, décodeur deflate, CRC-32.
 *
 * Déroulement : trouver l'enregistrement de fin d'annuaire (EOCD) -> parcourir l'annuaire central pour
 * la première entrée .gb/.gbc -> lire l'en-tête local -> décompresser (ou copier si stocké) directement
 * dans le tampon final -> vérifier le CRC-32. Le fichier est lu par blocs de 4 Ko : le zip n'est jamais
 * chargé en entier en mémoire (seule la ROM décompressée l'est).
 *
 * Le décodeur deflate (RFC 1951) décode les codes de <= 9 bits par table directe, les plus longs par
 * décodage canonique bit à bit.
 *
 * Projet  : gb-gnuboy-aka — lecteur Game Boy / Game Boy Color pour la Gamebuino AKA (ESP32-S3).
 * Auteur  : Jicehel (Jicehel-Aka)
 * Licence : voir README.md (cœur gnuboy : GPL v2 (components/gnuboy/COPYING) ; composant gamebuino : LGPL).
 */
#include "gbrt_zip.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* CRC-32 (polynôme 0xEDB88320)                                        */
/* ------------------------------------------------------------------ */

static uint32_t g_crc_table[256];
static int g_crc_ready;

/* Met à jour un CRC-32 avec len octets (table construite au premier appel). */
static uint32_t crc32_update(uint32_t crc, const uint8_t *p, size_t len) {
    if (!g_crc_ready) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            g_crc_table[i] = c;
        }
        g_crc_ready = 1;
    }
    crc = ~crc;
    while (len--) crc = g_crc_table[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

/* ------------------------------------------------------------------ */
/* Décodeur deflate                                                    */
/* ------------------------------------------------------------------ */

#define FAST_BITS 9
#define MAXBITS 15

/* Table de Huffman canonique : count[len] = nombre de codes de cette longueur, symbol[] triés par (longueur, valeur),
 * fast[] = accès direct pour les codes <= FAST_BITS ((longueur << 10) | symbole, 0 si absent). */
typedef struct {
    uint16_t count[MAXBITS + 1];
    uint16_t symbol[288];
    uint16_t fast[1 << FAST_BITS];
} Huff;

/* Lecteur de bits sur un FILE, limité à `remain` octets compressés. */
typedef struct {
    FILE *f;
    size_t remain;      /* octets compressés pas encore lus du fichier */
    uint8_t buf[4096];
    size_t pos, len;    /* position / taille utile dans buf */
    uint32_t bits;      /* tampon de bits (LSB en premier) */
    int nbits;
    int err;            /* 1 = lecture impossible ou flux tronqué */
} BitIn;

/* Complète le tampon de bits avec des octets du fichier (sans erreur si le flux est épuisé). */
static void refill(BitIn *b) {
    while (b->nbits <= 24) {
        if (b->pos == b->len) {
            if (b->remain == 0) return;
            size_t want = b->remain < sizeof(b->buf) ? b->remain : sizeof(b->buf);
            size_t got = fread(b->buf, 1, want, b->f);
            if (got == 0) { b->err = 1; b->remain = 0; return; }
            b->remain -= got; b->pos = 0; b->len = got;
        }
        b->bits |= (uint32_t)b->buf[b->pos++] << b->nbits;
        b->nbits += 8;
    }
}

/* Lit n (0..16) bits ; positionne err si le flux est trop court. */
static uint32_t getbits(BitIn *b, int n) {
    if (b->nbits < n) refill(b);
    if (b->nbits < n) { b->err = 1; return 0; }
    uint32_t v = b->bits & ((1u << n) - 1u);
    b->bits >>= n; b->nbits -= n;
    return v;
}

/* Inverse les n bits de poids faible de v. */
static uint32_t rev_bits(uint32_t v, int n) {
    uint32_t r = 0;
    while (n--) { r = (r << 1) | (v & 1); v >>= 1; }
    return r;
}

/* Construit une table de Huffman à partir des longueurs de codes ; 0 si ok, -1 si sur-souscrite.
 * Un code incomplet est toléré (une erreur n'apparaît que si le flux l'utilise). */
static int huff_build(Huff *h, const uint8_t *lens, int n) {
    memset(h->count, 0, sizeof(h->count));
    memset(h->fast, 0, sizeof(h->fast));
    for (int i = 0; i < n; ++i) h->count[lens[i]]++;
    h->count[0] = 0;
    int left = 1;
    for (int l = 1; l <= MAXBITS; ++l) {
        left <<= 1;
        left -= h->count[l];
        if (left < 0) return -1;
    }
    uint16_t offs[MAXBITS + 2], next[MAXBITS + 2];
    uint32_t code = 0;
    offs[1] = 0;
    next[1] = 0;
    for (int l = 1; l <= MAXBITS; ++l) {
        code = (code + h->count[l - 1]) << 1;       /* count[0] vaut 0 */
        next[l] = (uint16_t)code;
        offs[l + 1] = (uint16_t)(offs[l] + h->count[l]);
    }
    uint16_t o[MAXBITS + 2];
    memcpy(o, offs, sizeof(o));
    for (int s = 0; s < n; ++s) {
        int l = lens[s];
        if (!l) continue;
        h->symbol[o[l]++] = (uint16_t)s;
        uint32_t c = next[l]++;
        if (l <= FAST_BITS) {
            uint32_t r = rev_bits(c, l);
            for (uint32_t i = r; i < (1u << FAST_BITS); i += 1u << l)
                h->fast[i] = (uint16_t)((l << 10) | s);
        }
    }
    return 0;
}

/* Décode un symbole ; -1 en cas d'erreur (code invalide ou flux tronqué). */
static int huff_decode(BitIn *b, const Huff *h) {
    if (b->nbits < MAXBITS) refill(b);
    uint16_t e = h->fast[b->bits & ((1u << FAST_BITS) - 1u)];
    if (e) {
        int l = e >> 10;
        if (l > b->nbits) { b->err = 1; return -1; }
        b->bits >>= l; b->nbits -= l;
        return e & 0x3FF;
    }
    int code = 0, first = 0, index = 0;
    uint32_t bits = b->bits;
    for (int l = 1; l <= MAXBITS; ++l) {
        if (l > b->nbits) { b->err = 1; return -1; }
        code |= (int)(bits & 1); bits >>= 1;
        int count = h->count[l];
        if (code - count < first) {
            b->bits >>= l; b->nbits -= l;
            return h->symbol[index + (code - first)];
        }
        index += count; first += count; first <<= 1; code <<= 1;
    }
    return -1;
}

static const uint16_t LEN_BASE[29] = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
static const uint8_t LEN_EXTRA[29] = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
static const uint16_t DIST_BASE[30] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
static const uint8_t DIST_EXTRA[30] = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};

/* Décode un bloc compressé (litéraux/longueurs + distances) dans out[*o..cap). 0 si ok. */
static int inflate_codes(BitIn *b, uint8_t *out, size_t cap, size_t *o, const Huff *lit, const Huff *dist) {
    size_t p = *o;
    for (;;) {
        int sym = huff_decode(b, lit);
        if (sym < 0) return -1;
        if (sym < 256) {
            if (p >= cap) return -1;
            out[p++] = (uint8_t)sym;
        } else if (sym == 256) {
            *o = p;
            return 0;
        } else {
            sym -= 257;
            if (sym >= 29) return -1;
            size_t len = LEN_BASE[sym] + getbits(b, LEN_EXTRA[sym]);
            int ds = huff_decode(b, dist);
            if (ds < 0 || ds >= 30) return -1;
            size_t d = DIST_BASE[ds] + getbits(b, DIST_EXTRA[ds]);
            if (b->err || d > p || len > cap - p) return -1;
            if (d >= len) { memcpy(out + p, out + p - d, len); p += len; }
            else while (len--) { out[p] = out[p - d]; ++p; }
        }
    }
}

/* Ordre des longueurs de codes dans l'en-tête d'un bloc dynamique. */
static const uint8_t CL_ORDER[19] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};

/* Décompresse un flux deflate complet dans out (taille exacte cap attendue). 0 si ok, -1 sinon. */
static int inflate_stream(BitIn *b, uint8_t *out, size_t cap) {
    Huff *lit = (Huff *)malloc(sizeof(Huff));
    Huff *dist = (Huff *)malloc(sizeof(Huff));
    if (!lit || !dist) { free(lit); free(dist); return -2; }
    size_t o = 0;
    int rc = 0, last = 0;
    while (!last && rc == 0) {
        last = (int)getbits(b, 1);
        int type = (int)getbits(b, 2);
        if (b->err) { rc = -1; break; }
        if (type == 0) { /* bloc stocké */
            getbits(b, b->nbits & 7);
            uint32_t len = getbits(b, 16), nlen = getbits(b, 16);
            if (b->err || (len ^ 0xFFFFu) != nlen || len > cap - o) { rc = -1; break; }
            for (uint32_t i = 0; i < len; ++i) out[o++] = (uint8_t)getbits(b, 8);
            if (b->err) rc = -1;
        } else if (type == 1 || type == 2) {
            uint8_t lens[320];
            int nlit, ndist;
            if (type == 1) { /* codes fixes */
                int i = 0;
                for (; i < 144; ++i) lens[i] = 8;
                for (; i < 256; ++i) lens[i] = 9;
                for (; i < 280; ++i) lens[i] = 7;
                for (; i < 288; ++i) lens[i] = 8;
                nlit = 288;
                for (i = 0; i < 30; ++i) lens[288 + i] = 5;
                ndist = 30;
            } else {
                nlit = (int)getbits(b, 5) + 257;
                ndist = (int)getbits(b, 5) + 1;
                int ncl = (int)getbits(b, 4) + 4;
                if (b->err || nlit > 286 || ndist > 30) { rc = -1; break; }
                uint8_t cl[19] = {0};
                for (int i = 0; i < ncl; ++i) cl[CL_ORDER[i]] = (uint8_t)getbits(b, 3);
                if (b->err || huff_build(lit, cl, 19) != 0) { rc = -1; break; }
                int idx = 0;
                while (idx < nlit + ndist) {
                    int s = huff_decode(b, lit);
                    if (s < 0) { rc = -1; break; }
                    if (s < 16) lens[idx++] = (uint8_t)s;
                    else {
                        int rep, val = 0;
                        if (s == 16) { if (idx == 0) { rc = -1; break; } val = lens[idx - 1]; rep = 3 + (int)getbits(b, 2); }
                        else if (s == 17) rep = 3 + (int)getbits(b, 3);
                        else rep = 11 + (int)getbits(b, 7);
                        if (b->err || idx + rep > nlit + ndist) { rc = -1; break; }
                        while (rep--) lens[idx++] = (uint8_t)val;
                    }
                }
                if (rc) break;
                /* lens[] = nlit longueurs litéraux puis ndist longueurs distances : on les décale à 288 pour la suite */
                memmove(lens + 288, lens + nlit, (size_t)ndist);
                memset(lens + nlit, 0, (size_t)(288 - nlit));
                nlit = 288;
            }
            if (huff_build(lit, lens, nlit) != 0 || huff_build(dist, lens + 288, ndist) != 0) { rc = -1; break; }
            if (inflate_codes(b, out, cap, &o, lit, dist) != 0) rc = -1;
        } else {
            rc = -1;
        }
    }
    free(lit);
    free(dist);
    if (rc == 0 && o != cap) rc = -1;
    return rc;
}

/* ------------------------------------------------------------------ */
/* Structure du zip                                                    */
/* ------------------------------------------------------------------ */

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

int gbrt_zip_has_ext(const char *name) {
    const char *dot = name ? strrchr(name, '.') : NULL;
    return dot && tolower((unsigned char)dot[1]) == 'z' && tolower((unsigned char)dot[2]) == 'i' &&
           tolower((unsigned char)dot[3]) == 'p' && dot[4] == 0;
}

/* Vrai si le nom d'entrée est une ROM utilisable : .gb/.gbc, ni dossier, ni fichier caché, ni __MACOSX. */
static int entry_is_rom(const char *name) {
    size_t n = strlen(name);
    if (n == 0 || name[n - 1] == '/' || strncmp(name, "__MACOSX", 8) == 0) return 0;
    const char *base = name;
    for (const char *p = name; *p; ++p) if (*p == '/' || *p == '\\') base = p + 1;
    if (base[0] == '.') return 0;
    const char *dot = strrchr(base, '.');
    if (!dot) return 0;
    if (tolower((unsigned char)dot[1]) != 'g' || tolower((unsigned char)dot[2]) != 'b') return 0;
    return dot[3] == 0 || (tolower((unsigned char)dot[3]) == 'c' && dot[4] == 0);
}

/* Lit exactement n octets à la position pos du fichier ; 0 si ok. */
static int read_at(FILE *f, long pos, void *dst, size_t n) {
    if (fseek(f, pos, SEEK_SET) != 0) return -1;
    return fread(dst, 1, n, f) == n ? 0 : -1;
}

int gbrt_zip_read_rom(const char *path, uint8_t **out, size_t *size, size_t max_size,
                      char *inner_name, size_t inner_cap) {
    if (!path || !out || !size) return GBRT_ZIP_ERR_FORMAT;
    *out = NULL; *size = 0;
    if (inner_name && inner_cap) inner_name[0] = 0;

    FILE *f = fopen(path, "rb");
    if (!f) return GBRT_ZIP_ERR_IO;
    int res = GBRT_ZIP_ERR_FORMAT;
    uint8_t *rom = NULL;

    /* 1. Fin d'annuaire (EOCD) : dans les 64 Ko + 22 derniers octets. */
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return GBRT_ZIP_ERR_IO; }
    long fsize = ftell(f);
    if (fsize < 22) { fclose(f); return GBRT_ZIP_ERR_FORMAT; }
    size_t win = fsize < 65557 ? (size_t)fsize : 65557;   /* 22 octets d'EOCD + commentaire <= 65535 */
    uint8_t *tailbuf = (uint8_t *)malloc(win);
    if (!tailbuf) { fclose(f); return GBRT_ZIP_ERR_NOMEM; }
    uint8_t tail[22];
    int found = 0;
    if (read_at(f, fsize - (long)win, tailbuf, win) == 0) {
        for (long i = (long)win - 22; i >= 0; --i)
            if (rd32(tailbuf + i) == 0x06054b50u) { memcpy(tail, tailbuf + i, 22); found = 1; break; }
    }
    free(tailbuf);
    if (!found) { fclose(f); return GBRT_ZIP_ERR_FORMAT; }
    unsigned entries = rd16(tail + 10);
    uint32_t cd_off = rd32(tail + 16);
    if (entries == 0xFFFF || cd_off == 0xFFFFFFFFu) { fclose(f); return GBRT_ZIP_ERR_FORMAT; } /* ZIP64 */

    /* 2. Annuaire central : première entrée .gb/.gbc. */
    uint32_t pos = cd_off;
    uint8_t h[46];
    char name[256];
    int have = 0;
    uint16_t flags = 0, method = 0;
    uint32_t crc = 0, csize = 0, usize = 0, lofs = 0;
    for (unsigned i = 0; i < entries; ++i) {
        if (read_at(f, (long)pos, h, sizeof(h)) != 0 || rd32(h) != 0x02014b50u) { res = GBRT_ZIP_ERR_FORMAT; goto done; }
        unsigned nlen = rd16(h + 28), elen = rd16(h + 30), clen = rd16(h + 32);
        if (nlen < sizeof(name)) {
            if (read_at(f, (long)pos + 46, name, nlen) != 0) { res = GBRT_ZIP_ERR_IO; goto done; }
            name[nlen] = 0;
            if (entry_is_rom(name)) {
                flags = rd16(h + 8); method = rd16(h + 10);
                crc = rd32(h + 16); csize = rd32(h + 20); usize = rd32(h + 24); lofs = rd32(h + 42);
                have = 1;
                break;
            }
        }
        pos += 46u + nlen + elen + clen;
    }
    if (!have) { res = GBRT_ZIP_ERR_NO_ROM; goto done; }
    if (inner_name && inner_cap) { strncpy(inner_name, name, inner_cap - 1); inner_name[inner_cap - 1] = 0; }
    if ((flags & 1u) || (method != 0 && method != 8) || csize == 0xFFFFFFFFu || usize == 0xFFFFFFFFu) {
        res = GBRT_ZIP_ERR_FORMAT; goto done;   /* chiffré, méthode non gérée ou ZIP64 */
    }
    if (usize < 0x150 || usize > max_size) { res = GBRT_ZIP_ERR_SIZE; goto done; }

    /* 3. En-tête local -> début des données. */
    uint8_t lh[30];
    if (read_at(f, (long)lofs, lh, sizeof(lh)) != 0 || rd32(lh) != 0x04034b50u) { res = GBRT_ZIP_ERR_FORMAT; goto done; }
    long data = (long)lofs + 30 + rd16(lh + 26) + rd16(lh + 28);
    if (fseek(f, data, SEEK_SET) != 0 || (long)csize > fsize - data) { res = GBRT_ZIP_ERR_IO; goto done; }

    rom = (uint8_t *)malloc(usize);
    if (!rom) { res = GBRT_ZIP_ERR_NOMEM; goto done; }
    if (method == 0) {
        if (csize != usize || fread(rom, 1, usize, f) != usize) { res = GBRT_ZIP_ERR_CORRUPT; goto done; }
    } else {
        BitIn *b = (BitIn *)calloc(1, sizeof(BitIn));
        if (!b) { res = GBRT_ZIP_ERR_NOMEM; goto done; }
        b->f = f; b->remain = csize;
        int rc = inflate_stream(b, rom, usize);
        free(b);
        if (rc == -2) { res = GBRT_ZIP_ERR_NOMEM; goto done; }
        if (rc != 0) { res = GBRT_ZIP_ERR_CORRUPT; goto done; }
    }
    if (crc32_update(0, rom, usize) != crc) { res = GBRT_ZIP_ERR_CORRUPT; goto done; }

    *out = rom; *size = usize; rom = NULL;
    res = GBRT_ZIP_OK;
done:
    free(rom);
    fclose(f);
    return res;
}
