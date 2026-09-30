SD_files — contenu a copier a la racine de la carte SD de la Gamebuino AKA
==========================================================================

SD_files/
  GB_EMULATOR/        la "cartouche" de l'emulateur pour le loader AKA
    firmware.bin      (a ajouter : produit par `idf.py build` -> build/gb_gnuboy_aka.bin, renomme en firmware.bin ;
                       la CI GitHub le place deja dans SD_files-<version>.zip)
    meta.json         titre, description, auteur, version, date affiches par le loader
    screen.bmp        image affichee par le loader (BMP 24 bits, 160x120)
    Picture.png       image du catalogue / ecran de demarrage (PNG, 320x240)
  GB/                 tes ROM : le lanceur parcourt /GB et tous ses sous-dossiers (.gb, .gbc et .zip contenant une ROM)
    Jeux_GB/          jeux Game Boy fournis pour les tests (8 ROM)
    GBC_Homebrew/     8 homebrew libres, dont 6 en mode couleur exclusif (licences dans LICENCES.txt)
    Gros_jeux_test/   5 jeux de 256 Ko a 1 Mo (MBC1/MBC2/MBC5, sauvegardes batterie, Pokemon Jaune en mode couleur)
  AKA/lang/           textes du menu systeme (fr, en, de, es, it) : communs a tous les jeux AKA
  GB_EMULATOR/lang/   textes propres a l'emulateur (commandes, volume...) ; prioritaires sur ceux de AKA/lang
  GBSAVES/            sauvegardes <nom_de_la_rom>.sav ; cree automatiquement s'il manque.
                      Les 5 .sav fournis correspondent aux ROM de Gros_jeux_test (memes noms), au format gnuboy (RAM par banques de 8 Ko ; MYSTICQU complete de zeros, non verifie en jeu).
  GB_EMULATOR/screenshots/  captures d'ecran (MENU maintenu 500 ms), creees par le menu systeme.

Noms longs : les noms de fichiers ne sont pas limites au format 8.3 (sdkconfig a parametrer comme pour tes autres jeux).
Droits d'auteur : Jeux_GB/ et Gros_jeux_test/ contiennent des jeux commerciaux ; ils sont exclus du depot git (.gitignore).
