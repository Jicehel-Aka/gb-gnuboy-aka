#!/usr/bin/env python3
"""Génère une ROM Game Boy Color synthétique (32 Ko) pour tester le mode couleur : aucune donnée protégée.

Ce qu'elle vérifie (résultat visible à l'écran, et dans la WRAM en 0xC000/0xC001) :
  - palettes de fond CGB (BCPS/BCPD) et d'objets (OCPS/OCPD) : 4 bandes de couleurs différentes
  - banque VRAM 1 : attributs de tuiles, et données de tuile lues dans la banque 1 (bande du bas = violet ; orange si ça échoue)
  - banques WRAM (SVBK) : barre du haut verte si OK, rouge sinon
  - passage en double vitesse (KEY1 + STOP) : barre du bas verte si OK, rouge sinon
  - un sprite blanc (palette d'objets)

usage: make_cgb_test_rom.py sortie.gbc

Projet : gb-gnuboy-aka (Gamebuino AKA). Auteur : Jicehel. Voir README.md.
"""
import sys

LOGO = bytes.fromhex(
    "CEED6666CC0D000B03730083000C000D0008111F8889000E"
    "DCCC6EE6DDDDD999BBBB67636E0EECCCDDDC999FBBB9333E")

class Asm:
    def __init__(self, base):
        self.b = bytearray(); self.base = base; self.labels = {}; self.fix = []
    def label(self, n): self.labels[n] = self.base + len(self.b)
    def emit(self, *x): self.b += bytes(x)
    def jr(self, op, n): self.emit(op, 0); self.fix.append((len(self.b) - 1, n))
    def w16(self, op, v): self.emit(op, v & 255, v >> 8)
    def call(self, n): self.emit(0xCD, 0, 0); self.fix.append((len(self.b) - 2, n, 'abs'))
    def resolve(self):
        for f in self.fix:
            if len(f) == 3:
                t = self.labels[f[1]]; self.b[f[0]] = t & 255; self.b[f[0] + 1] = t >> 8
            else:
                t = self.labels[f[1]]; off = t - (self.base + f[0] + 1)
                assert -128 <= off <= 127, f
                self.b[f[0]] = off & 255

def rgb(r, g, b): return r | (g << 5) | (b << 10)
def pal(*c): return b"".join(x.to_bytes(2, "little") for x in c)

BG_PALS = (
    pal(rgb(31,31,31), rgb(31,0,0),   rgb(0,31,0),  rgb(0,0,31)) +   # 0 : rouge
    pal(rgb(31,31,31), rgb(31,31,0),  rgb(0,31,0),  rgb(31,0,31)) +  # 1 : jaune
    pal(rgb(31,31,31), rgb(0,31,31),  rgb(0,31,0),  rgb(0,0,31)) +   # 2 : cyan
    pal(rgb(31,31,31), rgb(31,15,0),  rgb(0,31,0),  rgb(16,0,16)) +  # 3 : orange, ou violet (indice 3)
    pal(rgb(0,0,0),    rgb(0,28,0),   rgb(0,31,0),  rgb(0,0,31)) +   # 4 : vert  (test réussi)
    pal(rgb(0,0,0),    rgb(31,0,0),   rgb(0,31,0),  rgb(0,0,31)))    # 5 : rouge (test raté)
OBJ_PAL = pal(rgb(0,0,0), rgb(31,31,31), rgb(0,31,0), rgb(0,0,31))

a = Asm(0x150)
E = a.emit
E(0xF3)                                   # di
a.label("w"); E(0xF0, 0x44, 0xFE, 0x90); a.jr(0x38, "w")   # attendre le VBlank
E(0xAF, 0xE0, 0x40)                       # LCD éteint
# palettes de fond
E(0x3E, 0x80, 0xE0, 0x68); a.w16(0x21, 0); a.fix.append((len(a.b) - 2, "bgpal", 'abs')); E(0x06, len(BG_PALS))
a.label("l1"); E(0x2A, 0xE0, 0x69, 0x05); a.jr(0x20, "l1")
# palette d'objets
E(0x3E, 0x80, 0xE0, 0x6A); a.w16(0x21, 0); a.fix.append((len(a.b) - 2, "objpal", 'abs')); E(0x06, len(OBJ_PAL))
a.label("l2"); E(0x2A, 0xE0, 0x6B, 0x05); a.jr(0x20, "l2")
# banque 0 : tuile 0 = indice 1 partout, carte de tuiles = 0
E(0xAF, 0xE0, 0x4F)
a.w16(0x21, 0x8000); E(0x06, 8)
a.label("t0"); E(0x3E, 0xFF, 0x22, 0xAF, 0x22, 0x05); a.jr(0x20, "t0")
a.w16(0x21, 0x9800); a.w16(0x01, 1024)
a.label("m0"); E(0xAF, 0x22, 0x0B, 0x78, 0xB1); a.jr(0x20, "m0")
# banque 1 : tuile 0 = indice 3 partout, attributs par bandes de 4 lignes
E(0x3E, 0x01, 0xE0, 0x4F)
a.w16(0x21, 0x8000); E(0x06, 8)
a.label("t1"); E(0x3E, 0xFF, 0x22, 0x22, 0x05); a.jr(0x20, "t1")
a.w16(0x21, 0x9800)
for attr in (0x00, 0x01, 0x02, 0x0B):     # 0x0B = palette 3 + tuile lue en banque 1
    E(0x3E, attr, 0x06, 128); a.call("fill")
# test WRAM : SVBK 1 et 2 doivent être deux mémoires distinctes en 0xD000
E(0x3E, 0x01, 0xE0, 0x70, 0x3E, 0x11, 0xEA, 0x00, 0xD0)
E(0x3E, 0x02, 0xE0, 0x70, 0x3E, 0x22, 0xEA, 0x00, 0xD0)
E(0x3E, 0x01, 0xE0, 0x70, 0xFA, 0x00, 0xD0, 0xFE, 0x11); a.jr(0x20, "wf")
E(0x3E, 0x01, 0xEA, 0x00, 0xC0)
a.label("wf")
# test double vitesse : armer KEY1, STOP, lire KEY1 bit 7
E(0x3E, 0x30, 0xE0, 0x00, 0x3E, 0x01, 0xE0, 0x4D, 0x10, 0x00, 0xF0, 0x4D, 0xE6, 0x80); a.jr(0x28, "sf")
E(0x3E, 0x01, 0xEA, 0x01, 0xC0)
a.label("sf")
# barres de résultat (lignes 16 et 17) : palette 4 = vert, 5 = rouge
for addr, flag in ((0x9A00, 0xC000), (0x9A20, 0xC001)):
    E(0xFA, flag & 255, flag >> 8, 0xA7, 0x3E, 0x04); a.jr(0x20, "k%x" % addr); E(0x3E, 0x05)
    a.label("k%x" % addr); a.w16(0x21, addr); E(0x06, 20); a.call("fill")
# un sprite (tuile 0, palette d'objets 0)
E(0xAF, 0xE0, 0x4F)
a.w16(0x21, 0xFE00); E(0x3E, 0x50, 0x22, 0x3E, 0x58, 0x22, 0xAF, 0x22, 0x77)
E(0x3E, 0x93, 0xE0, 0x40)                 # LCD + fond + objets
a.label("end"); a.jr(0x18, "end")
a.label("fill"); E(0x22, 0x05); a.jr(0x20, "fill"); E(0xC9)
a.label("bgpal"); a.b += BG_PALS
a.label("objpal"); a.b += OBJ_PAL
a.resolve()

rom = bytearray(0x8000)
rom[0x100:0x104] = bytes([0x00, 0xC3, 0x50, 0x01])
rom[0x104:0x134] = LOGO
rom[0x134:0x13B] = b"CGBTEST"
rom[0x143] = 0x80                                        # compatible CGB
rom[0x147] = 0x00; rom[0x148] = 0x00; rom[0x149] = 0x00
assert 0x150 + len(a.b) < 0x8000
rom[0x150:0x150 + len(a.b)] = a.b
chk = 0
for x in rom[0x134:0x14D]: chk = (chk - x - 1) & 0xFF
rom[0x14D] = chk
g = (sum(rom) - rom[0x14E] - rom[0x14F]) & 0xFFFF
rom[0x14E], rom[0x14F] = g >> 8, g & 255
open(sys.argv[1], "wb").write(rom)
