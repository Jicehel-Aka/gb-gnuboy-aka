# ============================================================================
# Projet ESP-IDF racine : firmware « lecteur Game Boy » pour la Gamebuino AKA (ESP32-S3).
#
# Les composants du dossier components/ (gbrt_aka ici, gamebuino à copier depuis tes autres jeux AKA) sont ajoutés automatiquement.
# Projet : gb-gnuboy-aka (Gamebuino AKA). Auteur : Jicehel. Voir README.md.
# ============================================================================
cmake_minimum_required(VERSION 3.16)
set(EXTRA_COMPONENT_DIRS ${CMAKE_CURRENT_LIST_DIR}/components)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(gb_gnuboy_aka)
