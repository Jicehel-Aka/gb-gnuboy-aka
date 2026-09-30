# gb-gnuboy-aka — émulateur Game Boy / Game Boy Color pour Gamebuino AKA

Lecteur de ROM Game Boy et Game Boy Color pour la Gamebuino AKA (ESP32-S3), construit sur le cœur **gnuboy** de
[retro-go](https://github.com/ducalex/retro-go) (GPL v2), adapté pour l'AKA ([PATCHES-GNUBOY.md](PATCHES-GNUBOY.md)). C'est la variante « rapide » du projet `gb-recompiled-aka` (cœur gb-recompiled, plus exact mais ~4,7 fois plus lent sur PC).
Une version PC (Windows / Linux, SDL2) utilise exactement le même code d'émulation.

- Jeux **Game Boy** et **Game Boy Color** (mode couleur automatique d'après l'en-tête), mappers de gnuboy : ROM seule, MBC1, MBC2, MBC3 (avec horloge), MBC5 (vérifiés sur les ROM fournies) ; HuC1/HuC3 présents dans gnuboy mais **non testés** ; MBC6, MBC7, MMM01, Game Boy Camera non gérés.
- Son stéréo 44,1 kHz (mixé en mono sur l'AKA), sauvegarde batterie + horloge dans un seul `.sav` (format gnuboy / VBA).
- **ROM zippées** : un `.zip` contenant une ROM se lance comme un `.gb` (décompression à la volée, CRC vérifié) ; voir ci-dessous.
- Sélecteur de ROM avec sous-dossiers (`.gb`, `.gbc`, `.zip`), zoom 1x / 1,5x, saut d'affichage adaptatif.
- **Menu système AKA** (`aka_runtime`) : MENU court = menu (l'émulation est en pause) avec Reprendre, Choisir un jeu, Commandes, Langue, Volume, Crédits, Retour au loader ;
  MENU long (500 ms) = capture d'écran BMP ; RUN + MENU (500 ms) = retour au loader (la sauvegarde est écrite avant).

Documentation : [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) · [docs/ROMS.md](docs/ROMS.md) · [PATCHES-GNUBOY.md](PATCHES-GNUBOY.md) · [CHANGELOG.md](CHANGELOG.md).

## Ce qui change par rapport à gb-recompiled-aka

| | gb-recompiled-aka | gb-gnuboy-aka |
|---|---|---|
| Vitesse PC (médiane 21 ROM) | ~1 900 images/s | ~8 400 images/s (**×4,7** ; ×2,8 à ×6,4 selon le jeu) |
| Précision | test Blargg `instr_timing` réussi | `instr_timing` **échoue** ; 21 ROM jouées sans problème visible, mais moins exact sur les cas limites |
| Mémoire ROM | 2× la taille de la ROM à la lecture | 1× (gnuboy lit la ROM en place) ; ROM jusqu'à 8 Mo |
| Licence | MIT | **GPL v2** (à cause de gnuboy) — à vérifier avec la LGPL v3 du composant `gamebuino` ; ce n'est pas un avis juridique |
| Sauvegardes | `.sav` + `.rtc` | un seul `.sav` (gnuboy) ; pas de rattrapage de l'horloge MBC3 pendant l'arrêt |
| Son | niveau d'origine | ~1,6× plus faible (le menu Volume à 100 compense en partie) |

⚠ **Anciennes sauvegardes** : gnuboy attend la RAM par banques de 8 Ko : un ancien `.sav` de MBC2 (512 octets) n'est pas repris tel quel (celui fourni dans `SD_files/GBSAVES` a été complété de zéros jusqu'à 8 192 octets ; **non vérifié en jeu**), les autres formats n'ont **pas été testés** avec d'anciennes sauvegardes (prévois de les sauvegarder avant d'essayer) ; les `.rtc` sont ignorés.

## ROM zippées

Le sélecteur affiche aussi les fichiers `.zip` ; choisir l'un d'eux décompresse sa **première** entrée `.gb` ou `.gbc` (dossiers internes, fichiers cachés et `__MACOSX` ignorés) directement en mémoire, puis vérifie son CRC-32.
- Gérés : zip classique, compression deflate ou « stocké ». Refusés avec un message : ZIP64, zip chiffré, autres méthodes (Deflate64, LZMA, bzip2), zip sans ROM, zip corrompu.
- Un zip qui contient plusieurs ROM : seule la première est lancée (un zip = un jeu, comme les jeux de ROM « No-Intro » / GoodGB).
- Sauvegardes : `/GBSAVES/<nom_du_zip>.sav` (nom du zip sans `.zip`, raccourci + haché au-delà de 63 caractères) ; renommer le zip change donc le nom de la sauvegarde.
- Mémoire : la ROM décompressée reste en mémoire et gnuboy la lit en place (1 × la taille de la ROM en PSRAM).
- Vitesse : le décodeur est interne (`gbrt_zip.c`, sans dépendance, identique sur la console et sur PC). Sur PC une ROM de 2 Mo se décompresse en ~25 ms ; **non mesuré sur la console**.
- Vérifié sur PC : les 1659 zips du jeu de ROM fourni (Europe / USA / Japan / ...) donnent exactement le même contenu que Python (`zipfile`), et 25 d'entre eux ont été lancés 300 images sous ASAN/UBSAN ; 720 archives tronquées/altérées au hasard ne plantent pas.

## Installer sur la console

1. Les composants sont déjà dans `components/` : `gamebuino` (bibliothèque AKA), `aka_runtime`, `aka_font` et `gbrt_aka` (l'émulateur).
   ⚠ **`gamebuino` doit être la version récente** (celle de pAKAman / AKA-Love : elle a `gb_err.h` et `gb_buttons::set_run_power_off()`). L'ancienne version
   éteint la console à chaque appui sur RUN, or RUN sert ici de Start ; le lanceur refuse donc de compiler avec elle (message `#error`).
2. Compile comme un projet ESP-IDF habituel (`idf.py build`). `sdkconfig.defaults` et `partitions.csv` sont fournis (repris d'AKA-Love : flash 8 Mo, PSRAM octale, tick 1 ms,
   noms longs FAT, pile principale 8 Ko, optimisation -O2) ; si ta console utilise une autre table de partitions, garde la tienne (la partition `loader` OTA_1 doit rester celle du loader). Réglages de vitesse supplémentaires : `sdkconfig.perf.example`.
3. Copie le contenu de `SD_files/` à la racine de la carte SD : `GB_EMULATOR/` (la cartouche du loader : ajoute-y `build/gb_gnuboy_aka.bin` renommé `firmware.bin`)
   et `GB/` (tes ROM, en vrac ou rangées en sous-dossiers). `GBSAVES/` est créé tout seul. Détail : [SD_files/README.txt](SD_files/README.txt).

La CI GitHub produit tout cela : un tag `v*` publie `gb-gnuboy-aka-<version>.bin`, `SD_files-<version>.zip` (firmware + meta.json + images + homebrew libres),
et les archives Linux / Windows.

### Les `.bin` de la META

Les `.bin` de la Gamebuino META sont des firmwares ARM Cortex-M0+ : ils ne peuvent pas s'exécuter sur l'ESP32-S3 (Xtensa). Chacun contient en revanche la ROM Game Boy d'origine, copiée telle quelle.
`tools/extract_meta_rom.py` la retrouve (en-tête et checksums vérifiés) :

    python3 tools/extract_meta_rom.py solar_striker.bin solar_striker.gb

## Commandes

| Console AKA | Action |
|---|---|
| Croix / joystick, A, B | Croix, A, B |
| RUN | Start |
| C | Select |
| L1 | zoom 1x (centré) / 1,5x |
| MENU (appui court) | menu système : pause, Reprendre, Choisir un jeu (retour au sélecteur), Commandes, Langue, Volume, Crédits, Retour au loader |
| MENU (500 ms) | capture d'écran : `/sdcard/GB_EMULATOR/screenshots/NNNN.BMP` |
| RUN + MENU (500 ms) | retour au loader (la sauvegarde est écrite avant) |
| Sélecteur : haut/bas, A | choisir, lancer ou ouvrir un dossier |
| Sélecteur : B | dossier parent (L1/R1 : page précédente/suivante) |
| Sélecteur : MENU long ou RUN + MENU | retour au loader |

Sur PC : flèches ou WASD, X/Espace = A, Z/B = B, Entrée = Start, Retour arrière/Maj droite = Select, F11 plein écran, Échap = retour au lanceur ([pc/LISEZMOI-PC.txt](pc/LISEZMOI-PC.txt)).

## Sauvegardes

`/GBSAVES/<nom_de_la_rom>.sav` : RAM batterie + horloge MBC3 (format gnuboy, compatible VBA). Le nom est celui du fichier ROM sans dossier ni **dernière** extension ;
au-delà de 63 caractères il est raccourci et complété d'un hachage (deux ROM de même nom dans deux dossiers partagent donc leur sauvegarde : renomme l'une des deux).
Écriture différée (~2 s après le dernier changement), à la fermeture du jeu, quand tu choisis « Choisir un jeu » et avant un retour au loader. Elle est atomique (`.tmp` puis renommage : une coupure de courant garde l'ancienne sauvegarde).
L'horloge des cartouches MBC3 est celle du jeu : elle n'avance pas pendant que la console est éteinte.

## Versions PC et releases GitHub

- `pc/` : frontend SDL2 (Windows / Linux) avec le même lanceur à dossiers que l'AKA (dossier `roms/` à côté du programme). `gb_gnuboy_pc` ouvre le lanceur, `gb_recompiled_pc ma_rom.gb` lance une ROM directement.
  Compilation : `cmake -S pc -B build-pc && cmake --build build-pc` (paquet `libsdl2-dev`).
- `.github/workflows/build-pc.yml` : compile Linux et Windows (MSYS2/MinGW), lance les tests et un test de fumée avec une ROM synthétique, produit `.tar.gz` et `.zip`.
- `.github/workflows/build-aka.yml` : compile le firmware ESP32-S3 (`IDF_VERSION` en tête du fichier, v5.4 par défaut : mets la tienne) ; vérifie la présence de `components/gamebuino/`, de `sdkconfig.defaults` et de la table de partitions qu'il référence (tous fournis).
- `.github/workflows/release.yml` : sur un tag `v*` (ou lancement manuel), publie la release. Publier : `git tag v1.0.0 && git push origin v1.0.0`.

## Tests

    tools/run_tests.sh                      # tests unitaires + ROM synthétiques (Linux / MSYS2, aucune dépendance SDL)
    tools/run_tests.sh SD_files/GB 900      # + chaque ROM du dossier pendant 900 images sous ASAN/UBSAN

| Test | Vérifie |
|---|---|
| `tests/test_browser.c` | tri, filtre .gb/.gbc/.zip, navigation, limites du navigateur de dossiers |
| `tests/test_convert.c` | les 32 768 couleurs RGB555 donnent les mêmes 565 par le chemin direct et par le chemin RGBA |
| `tests/test_saveid.c` | nom de sauvegarde : dossiers, points multiples, noms longs |
| `tests/test_cgb.c` | mode couleur : palettes, banques VRAM/WRAM, double vitesse, sprites (ROM de `tools/make_cgb_test_rom.py`) |
| `tests/test_zip.c` | lecture des zips (deflate, stocké, CRC, archives corrompues) |
| `host_test/frameskip_test.c` | saut d'affichage adaptatif avec horloge virtuelle |
| `host_test/host_test.c` | banc PC sans SDL : vitesse, son, capture PPM d'une image |

## Vitesse (PC, 1500 images par ROM, sans limitation de cadence, images/s)

| ROM | gb-recompiled | gnuboy | gain |
|---|---|---|---|
| A_Slime_Travel.gbc | 1074 | 6047 | 5.6× |
| Aevilia.gbc | 2216 | 10383 | 4.7× |
| CatMario-GB.gbc | 938 | 5640 | 6.0× |
| CrossConnect.gbc | 2124 | 10799 | 5.1× |
| GBHack.gbc | 1984 | 10275 | 5.2× |
| Geometrix.gbc | 2049 | 9808 | 4.8× |
| Rebound.gbc | 1215 | 7092 | 5.8× |
| uCity.gbc | 1351 | 8483 | 6.3× |
| DONKEYKO.gb | 1597 | 7222 | 4.5× |
| LEGENDOF.gb | 2448 | 8624 | 3.5× |
| MYSTICQU.gb | 2340 | 7977 | 3.4× |
| POKEMONV.gb | 1452 | 7125 | 4.9× |
| SUPERDON.gb | 1406 | 9014 | 6.4× |
| Batman.gb | 1900 | 8495 | 4.5× |
| Burai_Fighter_Deluxe.gb | 2795 | 7842 | 2.8× |
| Felix_the_Cat_USA_Europe.gb | 1846 | 8439 | 4.6× |
| Gargoyles_Quest_USA_Europe.gb | 2376 | 8352 | 3.5× |
| Nemesis_Europe.gb | 1279 | 6942 | 5.4× |
| Noobow_Japan.gb | 2223 | 10510 | 4.7× |
| Solar_Striker.gb | 1488 | 6525 | 4.4× |
| Super_RC_Pro-Am.gb | 1937 | 7170 | 3.7× |

Médiane : ×4,7. Ces chiffres sont ceux d'un PC x86 : le gain relatif sur l'ESP32-S3 (Xtensa, caches, PSRAM) peut différer, et **rien n'est mesuré sur la console**.

## Optimisation

Déjà en place : sortie directe en RGB555 (aucune conversion par pixel côté cœur, puis RGB555 → BGR565 par tables dans `framebuffer`), code chaud de gnuboy en IRAM (option `GNUBOY_IRAM_EXTRA`, `-O3`),
WRAM / VRAM / image en SRAM interne, ROM en PSRAM lue en place, saut d'affichage adaptatif (2 images de suite au plus ; émulation et son continuent).

**Non vérifié sur la console** (ni build ESP-IDF complet ici) : écran, boutons, son, menu système, vitesse, mémoire, taille de l'IRAM. À surveiller :

- **Vitesse** : le rapport s'affiche toutes les 120 images sur la liaison série (`charge > 100 %` = trop lent). Si besoin : 240 MHz, désactiver `GNUBOY_IRAM_EXTRA` si l'IRAM déborde.
- **Mémoire** : image 160×144 + WRAM + VRAM en SRAM interne (~50 Ko) ; ROM en PSRAM.
- **Cadence** : `gb_graphics::update()` bloque jusqu'à la fin du transfert vers le LCD : ce temps s'ajoute à l'émulation de chaque image affichée (c'est ce que le saut d'affichage évite).
- **Menu système** : il lit `/sdcard/AKA/lang/<langue>.json` et `/sdcard/GB_EMULATOR/lang/<langue>.json` (fournis dans `SD_files/`). Sans eux le menu affiche les noms des clés.
  Une seule piste audio : seul le réglage « Volume » (ligne « Musique ») agit ; 80 = niveau d'origine, 100 = ×1,25 (saturé).
- **Audio** : gnuboy sort un signal plus faible (~1,6×) que l'ancien cœur ; monte le volume du menu si besoin.

## Limites connues

- Précision inférieure à gb-recompiled : le test Blargg `instr_timing` échoue (les ROM fournies ne montrent rien, mais certains jeux très sensibles au timing peuvent différer).
- Mappers : voir plus haut. Pas de Super Game Boy (les jeux « SGB Enhanced » tournent en Game Boy normal), pas de câble link, pas de sauvegarde d'état.
- Les noms de ROM de 96 caractères ou plus sont ignorés par le sélecteur ; 1024 entrées maximum par dossier (au-delà, « Liste tronquee » s'affiche : range les ROM en sous-dossiers).

## Licences

**Ce projet, dans cette variante, est distribué sous GPL v2** : le cœur gnuboy (`components/gnuboy`, © ses auteurs, voir `CREDITS` et `COPYING`) l'impose à l'ensemble du binaire.
Le composant `gamebuino` (LGPL v3, Gamebuino / Jean-Marie Papillon) est inclus avec ses en-têtes de licence ; vérifie la compatibilité de la combinaison GPL v2 / LGPL v3 avant de publier un binaire
(ce n'est pas un avis juridique). `aka_runtime` et `aka_font` sont les tiens. Si tu veux une licence permissive, garde la variante `gb-recompiled-aka` (MIT).
Les ROM homebrew de `SD_files/GB/GBC_Homebrew` gardent la licence de leurs auteurs ([LICENCES.txt](SD_files/GB/GBC_Homebrew/LICENCES.txt)).
Les jeux commerciaux fournis pour les tests (`Jeux_GB/`, `Gros_jeux_test/`) sont exclus du dépôt git par `.gitignore` : ne les publie pas.
