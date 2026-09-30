#!/usr/bin/env python3
"""make_zip_fixtures.py - fabrique les archives de test de tests/test_zip.c dans le dossier donne.

Chaque cas ecrit <nom>.zip ; expected.txt liste "<nom> <taille> <fnv1a>" (ROM attendue) ou "<nom> ERR <code>".
Codes : -1 IO, -2 FORMAT, -3 NO_ROM, -4 SIZE, -6 CORRUPT.
Projet gb-gnuboy-aka (Gamebuino AKA). Auteur : Jicehel.
"""
import os, random, struct, sys, zipfile

out = sys.argv[1]
os.makedirs(out, exist_ok=True)
random.seed(42)

def fnv(b):
    h = 2166136261
    for x in b:
        h = ((h ^ x) * 16777619) & 0xFFFFFFFF
    return h

def rom(n, rnd_ratio):
    """ROM synthetique : melange de zones aleatoires et repetitives (force blocs dynamiques ET correspondances longues)."""
    b = bytearray()
    while len(b) < n:
        if random.random() < rnd_ratio:
            b += bytes(random.randrange(256) for _ in range(random.randrange(16, 400)))
        else:
            b += bytes([random.randrange(256)]) * random.randrange(1, 300)
            b += bytes(range(random.randrange(256))) 
    return bytes(b[:n])

exp = []
def mk(name, entries, level=6, method=zipfile.ZIP_DEFLATED, want=None):
    p = os.path.join(out, name + '.zip')
    with zipfile.ZipFile(p, 'w', method, compresslevel=level) as z:
        for n, d in entries:
            z.writestr(n, d)
    if want is None:
        n, d = next((n, d) for n, d in entries if n.lower().endswith(('.gb', '.gbc')) and not n.startswith('__MACOSX') and not n.split('/')[-1].startswith('.'))
        want = '%d %08x' % (len(d), fnv(d))
    exp.append('%s %s' % (name, want))

big = rom(300000, 0.3)
small = rom(0x200, 0.5)
mk('deflate_dynamic', [('Jeu (Europe).gb', big)])
mk('deflate_level1', [('a.gbc', big)], level=1)
mk('deflate_level9', [('a.gb', big)], level=9)
mk('tiny_fixed_huffman', [('t.gb', bytes(0x150))])          # tres compressible -> codes fixes / courts
mk('stored', [('s.gb', small)], method=zipfile.ZIP_STORED)
mk('subfolder_and_junk', [('__MACOSX/._x.gb', b'junk' * 100), ('readme.txt', b'hi'), ('sub/.hidden.gb', b'x' * 400), ('sub/Real.GB', small)])
mk('two_roms_first_wins', [('z.txt', b'1'), ('first.gb', small), ('second.gb', big)])
mk('no_rom', [('readme.txt', b'pas de rom ici')], want='ERR -3')
mk('too_small', [('p.gb', b'x' * 0x100)], want='ERR -4')

# Cas corrompus : octets modifies dans un zip valide.
def mutate(name, src, fn, want):
    d = bytearray(open(os.path.join(out, src + '.zip'), 'rb').read())
    fn(d)
    open(os.path.join(out, name + '.zip'), 'wb').write(d)
    exp.append('%s %s' % (name, want))
mutate('bad_crc_stored', 'stored', lambda d: d.__setitem__(40, d[40] ^ 1) , 'ERR -6')
mutate('truncated', 'deflate_dynamic', lambda d: d.__delitem__(slice(len(d) // 2, len(d))), 'ERR -2')
mutate('corrupt_deflate_data', 'deflate_dynamic', lambda d: d.__setitem__(slice(200, 210), bytes(10)), 'ERR -6')
open(os.path.join(out, 'not_a_zip.zip'), 'wb').write(b'this is not a zip file at all' * 10)
exp.append('not_a_zip ERR -2')
open(os.path.join(out, 'encrypted_flag.zip'), 'wb').write(open(os.path.join(out, 'stored.zip'), 'rb').read())
d = bytearray(open(os.path.join(out, 'encrypted_flag.zip'), 'rb').read())
i = d.index(b'PK\x01\x02'); d[i + 8] |= 1          # drapeau "chiffre" dans l'annuaire central
open(os.path.join(out, 'encrypted_flag.zip'), 'wb').write(d)
exp.append('encrypted_flag ERR -2')
open(os.path.join(out, 'expected.txt'), 'w').write('\n'.join(exp) + '\n')
print('%d cas ecrits dans %s' % (len(exp), out))
