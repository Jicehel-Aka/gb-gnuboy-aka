#!/usr/bin/env python3
"""Extrait la ROM Game Boy embarquée dans un firmware .bin de la Gamebuino META
(produit par gbrecomp --target meta) pour la copier telle quelle sur la SD de l'AKA.

usage: extract_meta_rom.py firmware.bin [sortie.gb]

Projet : gb-gnuboy-aka (Gamebuino AKA). Auteur : Jicehel. Voir README.md.
"""
import struct, sys, os

# Logo Nintendo de l'en-tête Game Boy (0x104-0x133) : sert de signature pour retrouver la ROM dans le firmware.
LOGO = bytes.fromhex("CEED6666CC0D000B03730083000C000D0008111F8889000E")
# Octet 0x148 de l'en-tête -> taille de la ROM (32 Ko << n).
ROM_SIZES = {i: 32 * 1024 << i for i in range(9)}

def checks(rom):
    """Renvoie (checksum d'en-tête OK, checksum global OK) d'après les octets 0x14D et 0x14E-0x14F."""
    hc = 0
    for b in rom[0x134:0x14D]:
        hc = (hc - b - 1) & 0xFF
    gs = (sum(rom) - rom[0x14E] - rom[0x14F]) & 0xFFFF
    return hc == rom[0x14D], gs == struct.unpack(">H", rom[0x14E:0x150])[0]

def main():
    """Cherche le logo dans le firmware, recoupe taille et checksum d'en-tête, puis écrit la ROM trouvée."""
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    data = open(sys.argv[1], "rb").read()
    pos = -1
    while True:
        pos = data.find(LOGO, pos + 1)
        if pos < 0:
            sys.exit("Aucun en-tête Game Boy trouvé dans ce fichier.")
        base = pos - 0x104
        if base < 0 or base + 0x150 > len(data):
            continue
        size = ROM_SIZES.get(data[base + 0x148])
        if not size or base + size > len(data):
            continue
        rom = data[base:base + size]
        hdr_ok, glob_ok = checks(rom)
        if hdr_ok:
            break
    title = rom[0x134:0x144].split(b"\0")[0].decode("ascii", "replace")
    out = sys.argv[2] if len(sys.argv) > 2 else os.path.splitext(sys.argv[1])[0] + ".gb"
    open(out, "wb").write(rom)
    print(f"{title!r}: {size // 1024} Ko, MBC=0x{rom[0x147]:02X}, "
          f"checksum en-tête {'OK' if hdr_ok else 'KO'}, global {'OK' if glob_ok else 'KO'} -> {out}")

if __name__ == "__main__":
    main()
