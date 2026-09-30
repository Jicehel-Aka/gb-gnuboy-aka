# Journal des changements

## 1.0 — gb-gnuboy-aka : variante basée sur gnuboy (retro-go)

- Nouveau cœur : gnuboy (GPL v2) à la place de gb-recompiled : ×4,7 en médiane sur PC (×2,8 à ×6,4), ROM lue en place (1× en mémoire). Détail et limites (précision, licence) : README.
- Patchs de gnuboy : écrasement mémoire dans `wnd_scan` (WX < 0) corrigé, accès non alignés et décalages indéfinis corrigés, IRAM / SRAM interne, sortie RGB555, palette DMG de l'ancien cœur. Voir PATCHES-GNUBOY.md.
- Menu système AKA dans le lanceur : MENU court = pause + menu (Reprendre, Choisir un jeu, Commandes, Langue, Volume, Crédits, Retour au loader), MENU long = capture BMP, RUN+MENU = retour au loader.
  La RAM batterie est écrite avant tout retour au loader (`gbrt_aka_flush_save()`, crochet `AkaRuntime::setBeforeExitCallback`). Textes : `SD_files/AKA/lang`, `SD_files/GB_EMULATOR/lang`.
- Inchangés : sélecteur de ROM, zips, `.gb` / `.gbc`, frontend PC.
- Sauvegardes : un seul `.sav` au format gnuboy ; `.rtc` abandonné. Anciens `.sav` de MBC2 non repris.
- Mesuré : 21 ROM × 1500 images sous ASAN/UBSAN, tests unitaires, CGB, zips. **Non testé sur la console** ni compilé avec ESP-IDF.

## 2026-09-30 — revue complète, ROM de test, SD_files, documentation

**Corrections**
- **Compilation ESP-IDF (`-Werror=format`)** : sur ESP32-S3 `uint32_t` est `unsigned long`, donc les `%u`/`%d` passes a des `uint32_t`/`int32_t` faisaient echouer le build
  (`audio_stats.c`, `differential.c`, `gbrt.c`, et aussi `gbrt_port.c` / `gbrt_host_configuration.c`, non signales car le build s'arretait avant). Tous remplaces par `PRIu32`/`PRId32`/`SCNu32`
  (`<inttypes.h>`), y compris dans les macros `DIFF_FIELD` et le `sscanf` de la configuration hote. Verifie par `tools/check_esp_format.sh` (emule ce type sur PC, lance aussi par `run_tests.sh`). Sortie des ROM inchangee.
- **Pointeur dangling** (`gbrt.c`, `gb_context_try_load_rtc`) : `loaded_id` pouvait designer `legacy_title`, tableau declare dans un bloc `if` deja termine (comportement indefini, refuse par `-Werror=dangling-pointer`).
  Le tableau est maintenant declare au niveau de la fonction.
- **`release.yml`** : les fichiers testes au debut (`components/gamebuino/CMakeLists.txt`, `sdkconfig.defaults`, plus `partitions.csv`) sont maintenant dans le depot.
- **Identifiant de sauvegarde** : il était coupé au *premier* point du nom (`super.rc.pro.am.gb` -> `super.sav`,
  `solar.striker.gb` -> `solar.sav`, donc collisions possibles). Il ne retire plus que la *dernière* extension. Les noms de plus de 63 caractères
  (limite du runtime) sont raccourcis et complétés par un hachage, pour que deux variantes régionales aux noms longs aient chacune leur sauvegarde.
  `gbrt_aka_save_id_from_path()` est publique et testée (`tests/test_saveid.c`).
  ⚠ Les anciennes sauvegardes de ROM dont le nom contient un point (hors extension) sont à renommer : `super.sav` -> `super.rc.pro.am.sav`.
- **Cadence sur la console** (`main/aka_gb_main.cpp`) : l'attente de fin d'image passait par `vTaskDelay(pdMS_TO_TICKS(us/1000))`, qui vaut 0 tick
  quand il reste moins d'un tick : avec le tick par défaut d'ESP-IDF (10 ms) l'émulation pouvait ne jamais attendre. Avec `CONFIG_FREERTOS_HZ=1000` (tes projets, et `sdkconfig.defaults` ici)
  ce n'était pas un problème : la modification est une **précaution**, sans effet attendu à 1 ms. L'attente dort par ticks entiers puis cède le processeur jusqu'à l'échéance `esp_timer`.
  *Non testé sur la console.*
- **Composant `gamebuino` ancien** : la version fournie (sans `gb_err.h`) éteint la console à chaque appui sur RUN (`gb_buttons::update()` appelle `gb_ll_expander_power_off()`),
  donc Start = mort du jeu. Le projet embarque la version récente d'AKA-Love (RUN sans effet d'arrêt par défaut) et le lanceur appelle `set_run_power_off(false)` ; il refuse de compiler avec l'ancienne.
- **`GB_OK` / `GB_ERR`** : le lanceur les utilise maintenant via `gb_err.h` (contrat du mixeur : 0 = tampon rempli) ; `gb_core::init()` est testé (arrêt propre si le matériel est en échec).

**Ajouts**
- **ROM zippées** : le sélecteur accepte les `.zip` ; la première entrée `.gb`/`.gbc` est décompressée à la volée (`gbrt_zip.c/.h`, décodeur deflate interne, CRC-32 vérifié, ZIP64/chiffré/méthodes rares refusés). Nouveaux codes d'erreur `GBRT_AKA_ERR_ZIP`, `GBRT_AKA_ERR_ZIP_NO_ROM` et `gbrt_aka_strerror()` : le lanceur affiche un message lisible au lieu de « Erreur -N ». Tests : `tests/test_zip.c` + `tools/make_zip_fixtures.py` (14 cas, ASAN/UBSAN), comparaison .zip/.gb, étape CI ajoutée.
- Sélecteur : 1024 entrées par dossier au lieu de 512 (le dossier `Japan` du jeu de ROM zippées en a 773).
- `components/` complété : `gamebuino` (version récente), `aka_runtime`, `aka_font`. Le lanceur a été compilé (`-Wall -Wextra -Werror`) contre les vrais en-têtes du composant, avec de simples bouchons pour les seuls en-têtes ESP-IDF.
- `sdkconfig.defaults` et `partitions.csv` (repris d'AKA-Love) : noms longs FAT, tick 1 ms, pile principale 8 Ko (le défaut de 3,5 Ko est trop juste pour le lecteur), -O2, PSRAM octale.
- `SD_files/` : cartouche `GB_EMULATOR` (`meta.json`, `screen.bmp`, `Picture.png`), ROM de test, sauvegardes.
- 8 ROM homebrew libres (6 en mode couleur exclusif) + 8 jeux GB fournis (Solar Striker, Burai Fighter Deluxe, Super R.C. Pro-Am et Batman extraits des firmwares META `.bin` avec `tools/extract_meta_rom.py` ; les `.bin` de Felix the Cat et Gargoyle's Quest contiennent exactement les mêmes ROM que les `.gb` fournis, vérifié par empreinte) + 5 gros jeux de test.
- `tools/run_tests.sh` : lance tous les tests, et toutes les ROM d'un dossier sous ASAN/UBSAN.
- CI : test des identifiants de sauvegarde ; `SD_files-<version>.zip` (firmware + meta + images + homebrew libres) joint aux releases.
- En-tête de fichier et commentaires de fonctions sur tous les fichiers ; `docs/ARCHITECTURE.md`, `docs/ROMS.md`.

**Vérifié** (PC) : 21 ROM, image et son identiques avant/après, 0 erreur ASAN/UBSAN ; voir `docs/ROMS.md`.
