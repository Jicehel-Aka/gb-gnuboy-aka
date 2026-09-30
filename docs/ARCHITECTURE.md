# Architecture

```
  console AKA : main/aka_gb_main.cpp  sélecteur, écran, boutons, son          PC : pc/main_pc.c (SDL2)
        │  aka_runtime : menu système (MENU), capture (MENU long), loader (RUN+MENU)
        │  GbrtAkaHal (fonctions de l'hôte)                                        │
  ┌─────▼──────────────────────────────────────────────────────────────────────────▼─────┐
  │ components/gbrt_aka/src/gbrt_aka.c    boucle 59,73 Hz, entrées, .sav, conversions     │
  │ components/gbrt_aka/src/gbrt_browser.c  navigation dossiers (partagée AKA / PC)       │
  │ components/gbrt_aka/src/gbrt_zip.c      ROM dans un .zip : annuaire, deflate, CRC-32  │
  └─────┬─────────────────────────────────────────────────────────────────────────────────┘
        │ API gnuboy (gnuboy_init / load_rom / run / set_pad / load_sram / save_sram)
  ┌─────▼─────────────────────────────────────────────────────────────────────────────────┐
  │ components/gnuboy/   cpu.c (SM83)  hw.c (bus, MBC, timers, DMA)  lcd.c  sound.c  (GPL v2) │
  └──────────────────────────────────────────────────────────────────────────────────────┘
```

## Les couches

**Hôte** (`main/aka_gb_main.cpp` sur la console, `pc/main_pc.c` sur PC) : dessine le sélecteur, lit les boutons, affiche l'image, joue le son.
Il fournit à la couche suivante une structure `GbrtAkaHal` :

| Champ | Rôle |
|---|---|
| `read_buttons` | masque `GBRT_AKA_BTN_*` (1 = appuyé) — obligatoire. Sur la console, c'est aussi ici que passent `aka_runtime` et le menu système |
| `time_us`, `sleep_us` | horloge et attente de cadence — obligatoires |
| `present_rgb555` (ou `present_rgba`, `present`) | reçoit l'image 160x144 ; `present_rgb555` est le chemin direct (le cœur produit déjà du RGB555) |
| `audio_write` | échantillons stéréo 44 100 Hz entrelacés, une image à la fois |
| `should_quit` | vrai pour quitter proprement (écrit la RAM batterie) — utilisé par « Choisir un jeu » |
| `auto_frameskip`, `report`, `pixel_bgr565` | saut d'affichage, rapport de charge, format de pixel |

**Menu système** : `read_buttons` appelle `input_poll()` (seul lecteur du bus I2C/ADC) puis `akaRuntime.update()`. MENU court ouvre le menu : `run_system_menu()` boucle alors sur le menu, ce qui **fige l'émulation** (pause) et vide la piste audio.
À la fermeture la cadence se recale seule (retard > 2 images). « Choisir un jeu » lève `g_change_game`, lu par `should_quit` : la partie se ferme (sauvegarde écrite) et le sélecteur revient.
« Retour au loader » et RUN+MENU redémarrent la console : `aka_runtime` appelle d'abord `gbrt_aka_flush_save()` (crochet `setBeforeExitCallback`, ajouté à `aka_runtime`).

**Couche AKA** (`gbrt_aka.c`) : `gbrt_aka_run(hal, rom, save_dir, max_frames)` lit la ROM (ou la décompresse d'un zip), complète à la taille déclarée avec 0xFF, initialise gnuboy (mode DMG ou CGB d'après `0x143`),
charge `<save_dir>/<nom>.sav`, puis boucle : entrées → `gnuboy_run()` (une image, 70 224 cycles) → affichage (sauté si en retard) → audio → attente jusqu'à l'échéance (16 742 µs).
La RAM batterie est écrite ~2 s après un changement, à la fermeture et avant un retour au loader.

**gnuboy** (`components/gnuboy/`) : l'émulation. Le PPU est rendu ligne par ligne (`lcd.c`) et la synchronisation se fait à la granularité de l'instruction : plus rapide que gb-recompiled, moins exact aux cas limites.
Modifications listées dans [PATCHES-GNUBOY.md](../PATCHES-GNUBOY.md).

## Mémoire (ESP32-S3)

| Bloc | Où | Pourquoi |
|---|---|---|
| WRAM (8 × 4 Ko) et VRAM (2 × 8 Ko) | SRAM interne (`gb_calloc_fast`) | accès à chaque instruction |
| Image 160×144 RGB555 (46 Ko) + tampon audio | SRAM interne | écrite par le PPU |
| ROM | PSRAM, lue en place (aucune copie) | lecture seule |
| `framebuffer` 320x240 BGR565 (153 600 o) | fourni par le composant `gamebuino` | écran |
| Piste audio | 16 Ko (8192 échantillons mono) | tampon circulaire |

Sans `gb_calloc_fast`, ESP-IDF enverrait en PSRAM tout bloc > 16 Ko.

## Fichiers du dépôt

| Chemin | Contenu |
|---|---|
| `main/aka_gb_main.cpp` | lanceur AKA |
| `components/gbrt_aka/src`, `include` | couche AKA, navigateur, zip, interface publique |
| `components/gnuboy` | cœur gnuboy (retro-go, GPL v2) |
| `components/gamebuino`, `aka_runtime`, `aka_font` | bibliothèque AKA (écran, boutons, son, SD), socle des portages + menu système, police accentuée |
| `sdkconfig.defaults`, `partitions.csv` | réglages carte AKA et table de partitions avec le loader OTA_1 |
| `pc/` | frontend SDL2 + police 8x8 |
| `tests/`, `host_test/` | tests automatiques et bancs de mesure |
| `tools/` | extraction de ROM des `.bin` META, ROM synthétiques, `run_tests.sh` |
| `SD_files/` | ce qui se copie sur la carte SD (cartouche `GB_EMULATOR`, textes du menu `AKA/lang`, ROM) |
| `.github/workflows/` | CI PC, CI console, release |
