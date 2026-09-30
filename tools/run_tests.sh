#!/usr/bin/env bash
# ============================================================================
# run_tests.sh — compile et lance tous les tests du projet gb-gnuboy-aka (Linux / MSYS2, sans SDL).
#
# Projet : gb-gnuboy-aka (Gamebuino AKA). Auteur : Jicehel.
#
# usage : tools/run_tests.sh [dossier_de_roms] [images]
#   sans argument : tests unitaires + ROMs synthétiques (navigateur, conversions, identifiants de sauvegarde,
#                   saut d'image, mode couleur) ;
#   dossier_de_roms : lance en plus CHAQUE .gb/.gbc/.zip du dossier (sous-dossiers compris) pendant `images` images
#                   (900 par défaut) sous ASAN/UBSAN, et signale toute ROM qui plante, échoue ou déclenche une erreur ASAN/UBSAN.
# Code de sortie : 0 si tout passe, 1 sinon.
# ============================================================================
set -u
cd "$(dirname "$0")/.."
R=components/gbrt_aka
OUT=build-tests
ROMS_DIR="${1:-}"
FRAMES="${2:-900}"
CFLAGS="-std=gnu11 -O2 -Wno-unused-function -I$R/include -Icomponents/gnuboy"
mkdir -p "$OUT"
fail=0
step() { printf '\n== %s\n' "$1"; }
run()  { "$@" || { echo "ECHEC : $*"; fail=1; }; }

step "Navigateur de dossiers (ASAN/UBSAN)"
gcc -std=gnu11 -Wall -Wextra -fsanitize=address,undefined -I$R/include tests/test_browser.c $R/src/gbrt_browser.c -o $OUT/test_browser && run $OUT/test_browser

step "Conversions de couleur, identifiants de sauvegarde"
for t in test_convert test_saveid; do
  gcc $CFLAGS tests/$t.c $R/src/gbrt_aka.c $R/src/gbrt_zip.c components/gnuboy/*.c -o $OUT/$t -lm && run $OUT/$t
done

step "Saut d'affichage (ROM synthétique)"
python3 tools/make_test_rom.py $OUT/test.gb
gcc $CFLAGS host_test/frameskip_test.c $R/src/gbrt_aka.c $R/src/gbrt_zip.c components/gnuboy/*.c -o $OUT/frameskip_test -lm && run $OUT/frameskip_test $OUT/test.gb

step "Mode Game Boy Color (ROM synthétique)"
python3 tools/make_cgb_test_rom.py $OUT/test.gbc
gcc $CFLAGS tests/test_cgb.c $R/src/gbrt_aka.c $R/src/gbrt_zip.c components/gnuboy/*.c -o $OUT/test_cgb -lm && run $OUT/test_cgb $OUT/test.gbc

step "ROM dans un .zip (décodeur deflate, erreurs, même image qu'en .gb)"
python3 tools/make_zip_fixtures.py $OUT/zipfix > /dev/null
gcc -std=gnu11 -Wall -Wextra -g -fsanitize=address,undefined -I$R/include tests/test_zip.c $R/src/gbrt_zip.c -o $OUT/test_zip && run $OUT/test_zip $OUT/zipfix
python3 - "$OUT" <<'PY'
import sys, zipfile
o = sys.argv[1]
with zipfile.ZipFile(o + '/test_rom.zip', 'w', zipfile.ZIP_DEFLATED) as z:
    z.write(o + '/test.gb', 'Test Rom (zip).gb')
PY
gcc $CFLAGS host_test/host_test.c $R/src/gbrt_aka.c $R/src/gbrt_zip.c components/gnuboy/*.c -o $OUT/host_test_zip -lm
plain=$($OUT/host_test_zip $OUT/test.gb 120 2>&1 | grep -E '^rc=' | grep -oE 'audio=.*')
zipped=$($OUT/host_test_zip $OUT/test_rom.zip 120 2>&1 | grep -E '^rc=' | grep -oE 'audio=.*')
if [ -n "$plain" ] && [ "$plain" = "$zipped" ]; then echo "ok     .zip et .gb donnent le meme resultat ($plain)"; else echo "ECHEC : .gb='$plain' .zip='$zipped'"; fail=1; fi

if [ -n "$ROMS_DIR" ]; then
  step "Toutes les ROM de $ROMS_DIR ($FRAMES images, ASAN/UBSAN)"
  gcc -std=gnu11 -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined -Wno-unused-function \
      -I$R/include -Icomponents/gnuboy host_test/host_test.c $R/src/gbrt_aka.c $R/src/gbrt_zip.c components/gnuboy/*.c -o $OUT/host_test_asan -lm || fail=1
  count=0
  while IFS= read -r -d '' rom; do
    count=$((count+1))
    res=$($OUT/host_test_asan "$rom" "$FRAMES" 2>&1 | grep -E '^rc=')
    if [ -z "$res" ] || ! echo "$res" | grep -q '^rc=0 '; then echo "ECHEC  $rom : ${res:-plantage}"; fail=1
    else echo "ok     $(basename "$rom")  ${res#rc=0 }"; fi
  done < <(find "$ROMS_DIR" -type f \( -iname '*.gb' -o -iname '*.gbc' -o -iname '*.zip' \) -print0 | sort -z)
  echo "$count ROM testée(s)"
fi

sh tools/check_esp_format.sh || fail=1
echo
[ $fail -eq 0 ] && echo "TOUS LES TESTS PASSENT" || echo "DES TESTS ONT ECHOUE"
exit $fail
