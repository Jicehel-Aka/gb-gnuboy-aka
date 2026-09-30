# ============================================================================
# Composant ESP-IDF gbrt_aka : couche plateforme AKA (HAL, sauvegardes, cadence, image) + navigateur de ROM + lecteur de .zip,
# au-dessus du cœur gnuboy (composant voisin components/gnuboy). Nom historique conservé : lanceur, tests et CI inchangés.
#
# Projet : gb-gnuboy-aka (Gamebuino AKA). Auteur : Jicehel. Voir README.md.
# ============================================================================
idf_component_register(
    SRCS src/gbrt_aka.c src/gbrt_browser.c src/gbrt_zip.c
    INCLUDE_DIRS include
    REQUIRES gnuboy esp_timer heap
)
target_compile_options(${COMPONENT_LIB} PRIVATE -O2 -Wno-unused-function -Wno-unused-variable)
