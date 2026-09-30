# ROM fournies et résultats de test

Test du 30/09/2026 sur PC (`tools/run_tests.sh SD_files/GB 900`) : chaque ROM tourne 900 images sous ASAN/UBSAN avec des appuis Start/A espacés pour passer les écrans titre.
Les 21 ROM démarrent, affichent une image cohérente (contrôle visuel d'une capture à l'image 900 de chacune), produisent du son (sauf mention) et ne déclenchent aucune erreur ASAN/UBSAN.
Une seconde exécution (1 500 images, chemin RGB555 de la console) donne des empreintes d'image et de son **identiques** avant et après les corrections de ce lot.

Légende mode : **GB** = Game Boy classique · **GB+couleur** = compatible Game Boy Color (l'émulateur passe en mode couleur) · **GBC seul** = exige le mode couleur.

## SD_files/GB/GBC_Homebrew — tests du mode couleur (licences libres, voir LICENCES.txt)

| ROM | Mode | Mapper | Taille | Observé |
|---|---|---|---|---|
| Aevilia.gbc | GBC seul | MBC5+RAM+pile | 128 Ko | écran titre et dialogue OK |
| CatMario-GB.gbc | GBC seul | MBC5+RAM+pile | 128 Ko | niveau jouable, couleurs OK |
| CrossConnect.gbc | GB+couleur | MBC5+RAM+pile | 32 Ko | grille colorée OK |
| Geometrix.gbc | GB+couleur | MBC5+RAM+pile | 32 Ko | menu pause et plateau OK |
| uCity.gbc | GBC seul | MBC5+RAM+pile | 128 Ko | menu de sauvegardes OK |
| Rebound.gbc | GBC seul | MBC5 | 128 Ko | écran titre OK ; le son n'apparaît qu'après ~600 images |
| A_Slime_Travel.gbc | GBC seul | MBC5+RAM+pile | 256 Ko | écran titre OK ; **aucun son** mesuré sur 3 000 images avec les appuis automatiques (titre silencieux ? à vérifier à la main) |
| GBHack.gbc | GBC seul | MBC5+RAM+pile | 256 Ko | choix du compagnon OK |

## SD_files/GB/Jeux_GB — jeux fournis (Game Boy classique)

| ROM | Mode | Mapper | Taille | Origine |
|---|---|---|---|---|
| Nemesis_Europe.gb | GB | MBC1 | 128 Ko | `.gb` fourni |
| Noobow_Japan.gb | GB | MBC1 | 256 Ko | `.gb` fourni |
| Felix_the_Cat_USA_Europe.gb | GB | MBC1 | 128 Ko | `.gb` fourni (identique au contenu du `.bin`) |
| Gargoyles_Quest_USA_Europe.gb | GB | MBC1 | 128 Ko | `.gb` fourni (identique au contenu du `.bin`) |
| Batman.gb | GB | MBC1 | 128 Ko | extrait de `batman.bin` |
| Burai_Fighter_Deluxe.gb | GB | MBC1 | 64 Ko | extrait de `burai.fighter.deluxe.bin` |
| Solar_Striker.gb | GB | MBC1 | 64 Ko | extrait de `solar.striker.bin` |
| Super_RC_Pro-Am.gb | GB | MBC1 | 128 Ko | extrait de `super.rc.pro.am.bin` |

Les `.bin` sont des firmwares Gamebuino META (ARM) : seule la ROM qu'ils contiennent est utilisable ici (`tools/extract_meta_rom.py` vérifie en-tête et checksums).

## SD_files/GB/Gros_jeux_test — grosses ROM, sauvegardes batterie

| ROM | Mode | Mapper | Taille | Sauvegarde |
|---|---|---|---|---|
| DONKEYKO.gb | GB | MBC1+RAM+pile | 512 Ko | 8 Ko |
| LEGENDOF.gb | GB | MBC1+RAM+pile | 512 Ko | 8 Ko |
| MYSTICQU.gb | GB | MBC2+pile | 256 Ko | 512 o |
| POKEMONV.gb | GB+couleur | MBC5+RAM+pile | 1 Mo | 32 Ko |
| SUPERDON.gb | GB | MBC1+RAM+pile | 512 Ko | 8 Ko |

Les `.sav` de `SD_files/GBSAVES/` (nommés comme les ROM) se chargent sans erreur de taille ; Pokémon Jaune les relit et les réécrit à l'identique.
Les autres jeux modifient leur RAM au démarrage, ce qui est normal.

## À savoir

- Les tests ci-dessus sont des tests **sur PC**. Sur la console : vitesse, mémoire, son et boutons restent à valider (voir README).
- Il n'y a pas de ROM GBC commerciale dans le lot : le zip « Cyles Gameboy roms » ne contient que des jeux Game Boy, dont seulement cinq éditions de Pokémon Jaune, deux ROM « Unl » et un hack compatibles couleur.
  Le mode couleur exclusif est donc testé avec les homebrew libres ci-dessus, en plus de `tests/test_cgb.c` (pixels précis : palettes, banques VRAM/WRAM, double vitesse, sprites).
- Pokémon Jaune (`POKEMONV.gb`) est l'un des rares jeux du lot à s'exécuter en mode couleur sur une vraie ROM commerciale.
