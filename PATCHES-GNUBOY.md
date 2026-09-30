# Modifications apportées à gnuboy (GPL v2)

Source : [retro-go](https://github.com/ducalex/retro-go), `retro-core/components/gnuboy`, commit `4ced120` (gnuboy © Laguna, Rasmus Ilmonen, ducalex et autres :
voir `components/gnuboy/CREDITS`). Licence : GPL v2 (`components/gnuboy/COPYING`). Les fichiers modifiés portent un en-tête « MODIFIE par Jicehel »
renvoyant ici. Aucune modification ne change l'émulation elle-même, à l'exception de la palette DMG (choix de couleurs).

| Fichier | Changement | Pourquoi |
|---|---|---|
| `gnuboy.h` | `GB_HOT` (= `IRAM_ATTR` sur ESP32 si `CONFIG_GNUBOY_IRAM_EXTRA`) ; `gb_calloc_fast()` ; `GB_PIXEL_555` ; `gnuboy_free()` ; `MESSAGE_INFO` muet sauf `GNUBOY_VERBOSE` | code chaud en IRAM, WRAM/VRAM en SRAM interne, sortie RGB555 sans conversion, libération propre entre deux jeux |
| `gnuboy.c` | `gnuboy_free()` libère ROM, BIOS, banques RAM et VRAM | changer de jeu sans fuite mémoire |
| `lcd.c` | marges de 8 octets avant `BUF` et `PRI` ; format `GB_PIXEL_555` dans `sync_palette` ; `GB_HOT` sur `gb_lcd_emulate` | **corrige un écrasement mémoire** (`wnd_scan` écrit en `BUF + WX` avec `WX < 0` : « buffer overflow detected » sur 5 des 21 ROM avec glibc) |
| `hw.c` | `hw.rambanks` / `hw.vbanks` via `gb_calloc_fast` ; `GB_HOT` sur `gb_hw_read` / `gb_hw_write` | SRAM interne (sinon ESP-IDF envoie tout bloc > 16 Ko en PSRAM) |
| `hw.h` | `readw` / `writew` par `memcpy` | accès 16 bits non alignés indéfinis (Xtensa, UBSAN) |
| `sound.c` | décalages de valeurs négatives remplacés par des multiplications ; `GB_HOT` sur `gb_sound_emulate` | comportement indéfini (UBSAN) ; résultat identique |
| `tables.h` | `GB_PALETTE_DMG` : verts de l'ancien cœur (`0x67DC, 0x32EE, 0x2A66, 0x0841`) au lieu de l'olive de gnuboy | mêmes couleurs que la version précédente (0 pixel de différence vérifié) |
| `CMakeLists.txt`, `Kconfig` | composant ESP-IDF (`-O3`, option `GNUBOY_IRAM_EXTRA`) | intégration |

Vérification : 21 ROM × 1500 images sous ASAN/UBSAN sans erreur ; ROM de test CGB ; image de Batman identique à l'ancien cœur en DMG (palette).
