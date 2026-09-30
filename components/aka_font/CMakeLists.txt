# Composant aka_font : rendu de texte UTF-8 avec accents français/allemands/
# espagnols pour les jeux AKA (ESP32-S3, écran BGR565 ou tout framebuffer
# 16 bits). Header-only -- rien à compiler séparément, juste à inclure.
idf_component_register(
  INCLUDE_DIRS "include"
)
