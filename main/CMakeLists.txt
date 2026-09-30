# ============================================================================
# Composant principal : le lanceur aka_gb_main.cpp.
#
# Dépend de gbrt_aka (émulateur), gamebuino (écran/son/boutons), fatfs (carte SD), app_update/esp_partition (retour au loader), esp_psram.
# Projet : gb-gnuboy-aka (Gamebuino AKA). Auteur : Jicehel. Voir README.md.
# ============================================================================
idf_component_register(
    SRCS "aka_gb_main.cpp"
    REQUIRES gbrt_aka gamebuino aka_runtime fatfs esp_timer esp_psram app_update esp_partition
)
