#!/usr/bin/env python3
"""Génère une ROM Game Boy synthétique (32 Ko, sans MBC) pour les tests automatiques.
Elle allume le LCD, affiche un fond noir/blanc en damier et boucle : aucune donnée protégée.

usage: make_test_rom.py sortie.gb

Projet : gb-gnuboy-aka (Gamebuino AKA). Auteur : Jicehel. Voir README.md.
"""
import sys

LOGO = bytes.fromhex(
    "CEED6666CC0D000B03730083000C000D0008111F8889000E"
    "DCCC6EE6DDDDD999BBBB67636E0EECCCDDDC999FBBB9333E")

rom = bytearray(0x8000)
rom[0x100:0x104] = bytes([0x00, 0xC3, 0x50, 0x01])       # nop ; jp 0x0150
rom[0x104:0x134] = LOGO
rom[0x134:0x13B] = b"CITEST\0"
rom[0x147] = 0x00                                       # ROM seule
rom[0x148] = 0x00                                       # 32 Ko
rom[0x149] = 0x00
code = bytes([
    0xF0, 0x44, 0xFE, 0x90, 0x38, 0xFA,                  # attendre VBlank : ldh a,(LY) ; cp 144 ; jr c,-6
    0xAF, 0xE0, 0x40,                                    # xor a ; ldh (LCDC),a  (LCD éteint)
    0x3E, 0xE4, 0xE0, 0x47,                              # BGP = 0xE4
    0x21, 0x00, 0x80, 0x3E, 0xAA, 0x22, 0x3E, 0x55, 0x22, # tuile 0 : lignes alternées
    0x3E, 0x91, 0xE0, 0x40,                              # LCDC = 0x91 (LCD + BG on)
    0x18, 0xFE,                                          # boucle infinie : jr -2
])
rom[0x150:0x150 + len(code)] = code
chk = 0
for b in rom[0x134:0x14D]:
    chk = (chk - b - 1) & 0xFF
rom[0x14D] = chk
g = (sum(rom) - rom[0x14E] - rom[0x14F]) & 0xFFFF
rom[0x14E], rom[0x14F] = g >> 8, g & 0xFF
open(sys.argv[1], "wb").write(rom)
