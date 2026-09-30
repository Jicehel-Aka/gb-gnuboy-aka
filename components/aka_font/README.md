# aka_font

Rendu de texte UTF-8 pixel par pixel, avec support des accents français,
allemands et espagnols, pour les jeux AKA (ESP32-S3 + écran couleur).
Composant ESP-IDF header-only : pas de `.cpp` à compiler, juste à inclure.

## Pourquoi ce composant existe

`gb_graphics::print_str()` (composant `gamebuino`) ne connaît que
`font8x8_basic`, une police ASCII pure (U+0000-U+007F). Tout caractère
accentué (`é`, `ñ`, `ü`, `ß`...) s'affichait soit en `?`, soit pas du tout.
Ce composant fournit un rendu alternatif qui décode l'UTF-8 correctement et
choisit le bon glyphe (ASCII de base, ou accentué) plutôt que de planter ou
d'afficher du charabia.

## Ce qu'il couvre

41 caractères accentués en plus de l'ASCII de base (voir
`include/aka_font/font8x8_extended.h`) :

- **Français** : à â ç è é ê ë î ï ô ù û ü + majuscules correspondantes, «, »
- **Espagnol** : á í ñ ó ú + majuscules, ¿, ¡
- **Allemand** : ä ö ü ß + majuscules (ü/ö/ä partagés avec le français)

Non couvert (repli automatique, pas de plantage) :
- `œ`/`Œ` → affiché comme `oe`/`OE` (deux caractères)
- apostrophe/guillemets typographiques (`'` `"` `"`) → repli ASCII (`'` `"`)
- tout caractère hors de cette liste → `?`

## Comment les glyphes ont été construits

Pas une police tierce importée : chaque glyphe accentué part du glyphe ASCII
de base réel de `font8x8_basic` (domaine public) et lui ajoute l'accent dans
l'espace vide de la grille 8x8 (voir les commentaires détaillés en tête de
`tools/generate_extended_font.py`). Résultat : cohérent visuellement avec le
reste de la police, mais les accents sur majuscules sont plus sommaires que
sur minuscules (moins de place libre dans la grille).

## Utilisation

```cpp
#include "aka_font/gb_text_render.h"

// Dessiner du texte : fournir juste une fonction "allumer ce pixel"
gb_text::draw_utf8(x, y, "Grüße, ¡Hola!, Héllo", [&](int px, int py) {
    ma_fonction_set_pixel(px, py, ma_couleur);
});

// Largeur en pixels (compte les caractères, pas les octets UTF-8)
int w = gb_text::utf8_width("Café");  // -> 32 (4 caracteres x 8px), pas 40
```

Chasse fixe, 8px par caractère affiché, quelle que soit sa largeur réelle.

## Étendre à d'autres caractères/langues

1. Éditer `tools/generate_extended_font.py` : ajouter une entrée dans le
   dict `GLYPHS` en réutilisant les fonctions `accent_lower`/`accent_upper`
   existantes (acute/grave/circumflex/diaeresis/tilde/cedilla), ou dessiner
   un glyphe dédié à la main comme `ß`/`¿`/`¡`/`«`/`»`.
2. Relancer : `python3 tools/generate_extended_font.py` (chemins relatifs au
   script, marche depuis n'importe quel répertoire).
3. Recompiler les jeux qui utilisent ce composant.

## Intégration dans un projet ESP-IDF

Copier tout le dossier `aka_font/` dans `components/` du projet, puis dans
le `CMakeLists.txt` du composant qui en a besoin :

```cmake
idf_component_register(
    SRCS ...
    INCLUDE_DIRS "."
    REQUIRES aka_font ...
)
```

## Portage utilisant ce composant

- `wesnoth_sg` (portage Wesnoth "The South Guard") -- voir
  `components/wesnoth_sg/platform/gb_port_aka.cpp`, `gb_port_sdl.cpp`,
  `gb_port_host.cpp` pour trois exemples d'intégration (device réel, SDL
  PC, mock de test hôte).

## Licence

`font8x8_basic.h` : domaine public (Daniel Hepper / Marcel Sondaar / IBM,
voir l'en-tête du fichier). Les glyphes étendus (`font8x8_extended.h`) sont
dérivés de cette police de base par le studio.
