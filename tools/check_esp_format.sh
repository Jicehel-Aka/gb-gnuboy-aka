#!/bin/sh
# check_esp_format.sh - detecte sur PC les erreurs de format qui feraient echouer
# la compilation ESP-IDF (-Werror=format : sur ESP32, uint32_t = unsigned long, donc
# "%u" est refuse ; il faut PRIu32 / PRId32 / SCNu32 de <inttypes.h>).
# Emule ce type sur PC via tools/shim_esp32/. Sortie 0 = propre. Usage : sh tools/check_esp_format.sh
cd "$(dirname "$0")/.." || exit 2
R=components/gbrt_aka; rc=0
for f in components/gnuboy/*.c $R/src/*.c main/*.c; do
  [ -f "$f" ] || continue
  gcc -fsyntax-only -Itools/shim_esp32 -I$R/include -Icomponents/gnuboy \
      -std=gnu11 -O2 -Wformat -Wdangling-pointer -Werror=format -Werror=dangling-pointer "$f" || rc=1
done
[ $rc = 0 ] && echo "format ESP32 : OK" || echo "format ESP32 : ECHEC"
exit $rc
