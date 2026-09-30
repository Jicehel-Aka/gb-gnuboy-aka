// aka_runtime/aka_runtime.h — Socle commun a tous les portages AKA Port Studio.
// V0.2 : menu systeme complet (Reprendre/Commandes/Langue/Credits/Retour
// loader), gestion des langues, credits. Rendu directement via gb_graphics
// (independant de l'etat d'affichage propre a chaque jeu) -- reutilisable
// tel quel sur n'importe quel portage.
#pragma once
#include <cstdint>

struct Keys;   // core/input.h

class AkaRuntime {
public:
    void begin(const char* gameId);   // monte la SD, cree /sdcard/<gameId>/, charge settings.json

    // Informations pour l'ecran "Credits" du menu systeme -- valeurs fixes
    // au moment de la compilation (port_manifest.json n'est pas deploye sur
    // l'appareil), a appeler juste apres begin().
    void setCredits(const char* title, const char* author,
                     const char* license, const char* sourceUrl);

    // Liste des commandes du jeu, affichee dans "Commandes" -- chaque jeu
    // fournit ses propres cles de traduction (ex: {"CTRL_MOVE","CTRL_A",...}),
    // terminee par nullptr. Rendu generique (traduit automatiquement selon
    // la langue courante), a appeler juste apres begin().
    void setControlsKeys(const char* const* keys);

    // Optionnel : pour un jeu-collection (ex: crisp-game-lib, 267 mini-jeux)
    // qui a son propre selecteur de jeu interne -- si defini, une entree
    // "Choisir un jeu" apparait dans le menu systeme, juste apres
    // "Reprendre", et appelle cette fonction. Absent par defaut (les jeux
    // simples comme Kong-II n'ont pas ce concept).
    void setGameMenuCallback(void (*callback)());

    // A appeler une fois par frame ; renvoie true si le jeu doit continuer
    // (lire les boutons, dessiner), false si le menu systeme a pris la main
    // ce tour-ci (gere entierement ici, le jeu ne doit RIEN faire d'autre).
    bool update(const Keys& k);

    const char* gamePath() const;         // "/sdcard/<gameId>"
    const char* settingsPath() const;     // "/sdcard/AKA/settings.json"
    const char* screenshotPath() const;   // "/sdcard/AKA/screenshots"

    bool takeScreenshot();
    void returnToLoader();

    // Reste disponible pour un jeu qui veut un hook supplementaire (ex:
    // Karateka reutilise son propre ecran d'instructions visuel existant en
    // plus du "Commandes" generique du menu). Optionnel.
    bool helpRequested();

    // Vrai tant que le menu systeme est affiche -- permet a main.cpp de
    // savoir s'il doit rafraichir les touches lui-meme (cf. update()).
    bool isMenuOpen() const;

    // Langues (RUNTIME_SPEC.md) : fichiers plats {"CLE": "texte"} dans
    // /sdcard/AKA/lang/<code>.json (communes a tous les jeux) ET
    // /sdcard/AKA/lang/<gameId>/<code>.json (specifiques au jeu, prioritaires
    // -- permet de capitaliser les phrases partagees entre jeux tout en
    // gardant les commandes propres a chaque jeu). getLanguage() par defaut
    // "fr" ; translate() renvoie la cle elle-meme si absente (jamais de
    // chaine vide/crash).
    const char* getLanguage() const;
    void        setLanguage(const char* code);
    const char* translate(const char* key) const;

    uint8_t getMusicVolume() const;
    uint8_t getSfxVolume()   const;
    void    setMusicVolume(uint8_t v);
    void    setSfxVolume(uint8_t v);
    // Optionnel : chaque jeu a son propre lecteur audio, aka_runtime ne peut
    // pas l'atteindre directement -- ce callback est appele a chaque
    // changement de volume (menu "Volume") pour que le jeu applique la
    // nouvelle valeur a son propre son.
    void    setVolumeChangedCallback(void (*callback)(uint8_t musicVol, uint8_t sfxVol));

    // [gb-gnuboy-aka] Optionnel : appele juste avant le redemarrage vers le loader
    // (RUN+MENU ou entree "Retour au loader") pour que le jeu ecrive ses sauvegardes.
    void    setBeforeExitCallback(void (*callback)());

private:
    char m_gameId[32]   = {0};
    char m_gamePath[64] = {0};
    char m_language[8]  = "fr";

    const char* m_title  = "";
    const char* m_author = "";
    const char* m_license = "";
    const char* m_sourceUrl = "";
    const char* const* m_controlsKeys = nullptr;
    void (*m_gameMenuCallback)() = nullptr;

    // Menu systeme : etat + rendu (methodes privees, definies dans le .cpp).
    void menuDrawMain();
    void menuDrawControls();
    void menuDrawLanguage();
    void menuDrawCredits();
    void menuDrawVolume();
    bool menuHandleInput(const Keys& k);   // renvoie false quand le menu se ferme
};

extern AkaRuntime akaRuntime;
