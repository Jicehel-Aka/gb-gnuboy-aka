# ============================================================================
# Composant ESP-IDF gnuboy : cœur d'émulation Game Boy / Game Boy Color (gnuboy, tel qu'embarqué dans retro-go).
#
# Licence GPL v2 (voir COPYING) : le firmware qui inclut ce composant est distribué sous GPL v2 (voir README.md).
# Sans dépendance à retro-go. -O3 comme l'amont. Modifications AKA : PATCHES-GNUBOY.md à la racine du projet.
# Projet : gb-gnuboy-aka (Gamebuino AKA). Auteur : Jicehel.
# ============================================================================
idf_component_register(
    SRCS cpu.c gnuboy.c hw.c lcd.c sound.c
    INCLUDE_DIRS .
    REQUIRES esp_hw_support heap
)
target_compile_options(${COMPONENT_LIB} PRIVATE -O3 -Wno-unused-function -Wno-unused-variable)
