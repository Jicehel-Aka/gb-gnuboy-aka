#!/usr/bin/env python3
"""
generate_extended_font.py

Génère font8x8_extended.h : glyphes 8x8 pour les caractères accentués
français/allemands/espagnols, construits en ajoutant un accent à la lettre
de base réelle de font8x8_basic.h (pas de glyphes inventés à l'aveugle).

Usage : python3 generate_extended_font.py
(s'exécute depuis n'importe où -- les chemins sont relatifs à ce script,
pas au répertoire courant)

Technique :
- Minuscules (a,e,i,o,u,n,c) : les 2 lignes du haut (rangée 0 et 1) sont
  vides dans la police de base (vérifié empiriquement) -- l'accent y est
  dessiné sans toucher au corps de la lettre. Cas particulier du "i" : son
  point (rangée 0) est remplacé par l'accent, comme le font les vraies
  polices.
- Majuscules : aucune rangée libre en haut (la lettre occupe déjà toute la
  hauteur), mais la dernière rangée (7) est toujours vide -- on décale tout
  le glyphe d'une rangée vers le bas (perte nulle, la rangée 7 d'origine
  était déjà vide) pour libérer la rangée 0 pour un accent simplifié
  (1 rangée seulement, forcément moins net qu'en minuscule).
- Cédille (ç/Ç) : ajoute un petit crochet sur la rangée 7 (toujours vide
  dans la police de base), sans toucher au reste du glyphe -- aucun
  décalage nécessaire.
- ß, ¿, ¡, «, » : glyphes dédiés dessinés à la main (pas d'accent sur
  lettre de base, ce sont des caractères à part entière).
- œ/Œ : PAS de glyphe dédié -- trop complexe à faire lisible en 8x8 sans
  base existante à modifier ; repli sur "oe"/"OE" (deux caractères) au
  niveau du rendu (voir gb_text_render.h), pas ici.

Pour ajouter d'autres caractères (portugais, italien, etc.) : ajouter une
entrée dans GLYPHS ci-dessous en réutilisant les fonctions accent_lower/
accent_upper existantes, puis relancer ce script.
"""
import re
from pathlib import Path

FONT_DIR = Path(__file__).parent.parent / "include" / "aka_font"

def load_base():
    lines = (FONT_DIR / 'font8x8_basic.h').read_text().splitlines()
    rows = []
    for l in lines:
        m = re.match(r'\s*\{\s*(0x[0-9A-Fa-f]{2}(?:\s*,\s*0x[0-9A-Fa-f]{2}){7})\s*\}', l)
        if m:
            rows.append([int(x.strip(), 16) for x in m.group(1).split(',')])
    return {chr(i): rows[i] for i in range(128)}

BASE = load_base()

# --- Accents pour minuscules (rangées 0 et 1 libres, ou rangée 0 = point du i) ---
def acute_lower(letter):
    g = list(BASE[letter]); g[0] = 0b00001000; g[1] = 0b00000100; return g
def grave_lower(letter):
    g = list(BASE[letter]); g[0] = 0b00000100; g[1] = 0b00001000; return g
def circumflex_lower(letter):
    g = list(BASE[letter]); g[0] = 0b00001000; g[1] = 0b00010100; return g
def diaeresis_lower(letter):
    g = list(BASE[letter]); g[0] = 0; g[1] = 0b00010100; return g
def tilde_lower(letter):
    g = list(BASE[letter]); g[0] = 0b00100100; g[1] = 0b00011000; return g
def cedilla_lower(letter):
    g = list(BASE[letter]); g[7] = 0b00011000; return g

# --- Accents pour majuscules (decalees d'1 rangee vers le bas, 1 seule rangee libre en haut) ---
def shift_down(letter):
    g = list(BASE[letter])
    return [0] + g[0:7]  # perd g[7] (deja 0 dans la police de base)

def acute_upper(letter):
    g = shift_down(letter); g[0] = 0b00001000; return g
def grave_upper(letter):
    g = shift_down(letter); g[0] = 0b00000100; return g
def circumflex_upper(letter):
    g = shift_down(letter); g[0] = 0b00010100; return g
def diaeresis_upper(letter):
    g = shift_down(letter); g[0] = 0b00100010; return g  # ecartes pour distinguer du circonflexe
def tilde_upper(letter):
    g = shift_down(letter); g[0] = 0b00110110; return g
def cedilla_upper(letter):
    g = list(BASE[letter]); g[7] = 0b00011000; return g  # C majuscule a deja sa rangee 7 libre

GLYPHS = {}  # codepoint -> [8 bytes]

# Francais
GLYPHS[0x00E0] = grave_lower('a')       # à
GLYPHS[0x00E2] = circumflex_lower('a')  # â
GLYPHS[0x00E7] = cedilla_lower('c')     # ç
GLYPHS[0x00E8] = grave_lower('e')       # è
GLYPHS[0x00E9] = acute_lower('e')       # é
GLYPHS[0x00EA] = circumflex_lower('e')  # ê
GLYPHS[0x00EB] = diaeresis_lower('e')   # ë
GLYPHS[0x00EE] = circumflex_lower('i')  # î (remplace le point)
GLYPHS[0x00EF] = diaeresis_lower('i')   # ï
GLYPHS[0x00F4] = circumflex_lower('o')  # ô
GLYPHS[0x00F9] = grave_lower('u')       # ù
GLYPHS[0x00FB] = circumflex_lower('u')  # û
GLYPHS[0x00FC] = diaeresis_lower('u')   # ü (aussi allemand/espagnol)

GLYPHS[0x00C0] = grave_upper('A')       # À
GLYPHS[0x00C2] = circumflex_upper('A')  # Â
GLYPHS[0x00C7] = cedilla_upper('C')     # Ç
GLYPHS[0x00C8] = grave_upper('E')       # È
GLYPHS[0x00C9] = acute_upper('E')       # É
GLYPHS[0x00CA] = circumflex_upper('E')  # Ê
GLYPHS[0x00CB] = diaeresis_upper('E')   # Ë
GLYPHS[0x00CE] = circumflex_upper('I')  # Î
GLYPHS[0x00CF] = diaeresis_upper('I')   # Ï
GLYPHS[0x00D4] = circumflex_upper('O')  # Ô
GLYPHS[0x00D9] = grave_upper('U')       # Ù
GLYPHS[0x00DB] = circumflex_upper('U')  # Û
GLYPHS[0x00DC] = diaeresis_upper('U')   # Ü

# Espagnol (accents aigus deja definis ci-dessus pour e, ajout des autres voyelles + n)
GLYPHS[0x00E1] = acute_lower('a')       # á
GLYPHS[0x00ED] = acute_lower('i')       # í (remplace le point)
GLYPHS[0x00F3] = acute_lower('o')       # ó
GLYPHS[0x00FA] = acute_lower('u')       # ú
GLYPHS[0x00F1] = tilde_lower('n')       # ñ
GLYPHS[0x00C1] = acute_upper('A')       # Á
GLYPHS[0x00CD] = acute_upper('I')       # Í
GLYPHS[0x00D3] = acute_upper('O')       # Ó
GLYPHS[0x00DA] = acute_upper('U')       # Ú
GLYPHS[0x00D1] = tilde_upper('N')       # Ñ

# Glyphes dedies (pas d'accent sur lettre de base)
GLYPHS[0x00DF] = [0b00011100, 0b00100010, 0b00100010, 0b00101100,
                  0b00100010, 0b00100010, 0b00110100, 0b00000000]  # ß
GLYPHS[0x00BF] = [0b00001100, 0b00000000, 0b00001100, 0b00011000,
                  0b00110000, 0b00110011, 0b00011110, 0b00000000]  # ¿ (? inverse)
GLYPHS[0x00A1] = [0b00011000, 0b00000000, 0b00011000, 0b00011000,
                  0b00011000, 0b00011000, 0b00011000, 0b00000000]  # ¡ (! inverse)
# NB (2026-09-30) : ces deux bitmaps etaient echangees (« pointait a droite,
# » pointait a gauche -- bug decouvert via un rapport utilisateur "chevrons
# inverses" sur le texte francais). Corrige ici en plus du .h genere, pour
# qu'une regeneration future ne reintroduise pas le bug.
GLYPHS[0x00AB] = [0b00000000, 0b01001000, 0b00100100, 0b00010010,
                  0b00100100, 0b01001000, 0b00000000, 0b00000000]  # «
GLYPHS[0x00BB] = [0b00000000, 0b00010010, 0b00100100, 0b01001000,
                  0b00100100, 0b00010010, 0b00000000, 0b00000000]  # »

# --- Generation du header C++ ---
out = []
out.append('// font8x8_extended.h -- genere par generate_extended_font.py')
out.append('// Glyphes construits a partir des lettres de base reelles de font8x8_basic.h')
out.append('// (accent ajoute dans les rangees vides), pas de police tierce importee.')
out.append('#pragma once')
out.append('#include <cstdint>')
out.append('')
out.append('struct ExtGlyph { uint32_t codepoint; unsigned char bits[8]; };')
out.append('')
out.append('static const ExtGlyph font8x8_extended[] = {')
for cp in sorted(GLYPHS):
    bits = GLYPHS[cp]
    bits_str = ', '.join(f'0x{b:02X}' for b in bits)
    out.append(f'    {{ 0x{cp:04X}, {{ {bits_str} }} }},  // U+{cp:04X}')
out.append('};')
out.append(f'static const int font8x8_extended_count = {len(GLYPHS)};')
out.append('')

open(FONT_DIR / 'font8x8_extended.h', 'w').write('\n'.join(out) + '\n')
print(f"{len(GLYPHS)} glyphes generes -> {FONT_DIR / 'font8x8_extended.h'}")
