// platform/gb_text_render.h — Rendu texte UTF-8 partagé par les trois
// implémentations de la façade (device, SDL, mock hôte). Chaque plateforme
// fournit juste sa fonction de mise à un pixel ; le décodage UTF-8 et le
// choix du glyphe (police de base ou étendue) sont écrits une seule fois
// ici pour ne jamais diverger entre plateformes.
#pragma once
#include <cstdint>
#include <cstring>
#include "font8x8_basic.h"
#include "font8x8_extended.h"

namespace gb_text {

// Décode un codepoint UTF-8 (1 à 3 octets -- suffisant pour Latin-1
// Supplement + notre ponctuation typographique ; pas d'emoji/CJK à prévoir
// ici). Avance *p au premier octet du caractère suivant.
inline uint32_t utf8_next(const char*& p) {
    unsigned char c = (unsigned char)*p;
    if (c < 0x80) { p += 1; return c; }
    if ((c & 0xE0) == 0xC0 && p[1]) {
        uint32_t cp = ((c & 0x1F) << 6) | ((unsigned char)p[1] & 0x3F);
        p += 2; return cp;
    }
    if ((c & 0xF0) == 0xE0 && p[1] && p[2]) {
        uint32_t cp = ((c & 0x0F) << 12) | (((unsigned char)p[1] & 0x3F) << 6)
                      | ((unsigned char)p[2] & 0x3F);
        p += 3; return cp;
    }
    p += 1; return '?';  // octet invalide isolé -- on avance quand meme
}

// Ligatures non couvertes par un glyphe dédié (voir generate_extended_font.py) :
// repli sur deux caractères ASCII plutôt qu'un glyphe illisible en 8x8.
// Ponctuation typographique sans glyphe dédié : repli sur l'ASCII le plus proche.
inline int fallback_ascii(uint32_t cp, char out[2]) {
    switch (cp) {
        case 0x0153: out[0]='o'; out[1]='e'; return 2;  // œ
        case 0x0152: out[0]='O'; out[1]='E'; return 2;  // Œ
        case 0x2019: case 0x2018: out[0]='\''; return 1;
        case 0x201C: case 0x201D: out[0]='"';  return 1;
        case 0x2014: case 0x2013: out[0]='-';  return 1;
        default: out[0]='?'; return 1;
    }
}

// Renvoie le glyphe (8 octets) pour un codepoint, ou nullptr si aucun trouvé
// (l'appelant doit alors gérer fallback_ascii()).
inline const unsigned char* glyph_for(uint32_t cp) {
    if (cp < 128) return (const unsigned char*)font8x8_basic[cp];
    for (int i = 0; i < font8x8_extended_count; ++i) {
        if (font8x8_extended[i].codepoint == cp) return font8x8_extended[i].bits;
    }
    return nullptr;
}

// Dessine une chaîne UTF-8 en appelant set_pixel(x,y) pour chaque pixel
// allumé. Avance de 8px par caractère affiché (police à chasse fixe).
// Renvoie la largeur totale dessinée en pixels.
template <typename PixelFn>
int draw_utf8(int x, int y, const char* s, PixelFn set_pixel) {
    int cursor_x = x;
    const char* p = s;
    while (*p) {
        uint32_t cp = utf8_next(p);
        const unsigned char* glyph = glyph_for(cp);
        char fb[2] = {0, 0};
        int fb_len = 0;
        if (!glyph) fb_len = fallback_ascii(cp, fb);

        int chars_to_draw = glyph ? 1 : fb_len;
        for (int k = 0; k < chars_to_draw; ++k) {
            const unsigned char* g = glyph ? glyph : (const unsigned char*)font8x8_basic[(unsigned char)fb[k]];
            for (int dy = 0; dy < 8; ++dy) {
                unsigned char line = g[dy];
                for (int dx = 0; dx < 8; ++dx) {
                    if (line & 1) set_pixel(cursor_x + dx, y + dy);
                    line >>= 1;
                }
            }
            cursor_x += 8;
        }
    }
    return cursor_x - x;
}

// Largeur en pixels d'une chaîne UTF-8 (compte les codepoints, pas les
// octets -- un é sur 2 octets ne doit compter que pour 8px, pas 16).
inline int utf8_width(const char* s) {
    int n = 0;
    const char* p = s;
    while (*p) { utf8_next(p); n++; }
    return n * 8;
}

}  // namespace gb_text
