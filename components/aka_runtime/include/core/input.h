#pragma once
#include <cstdint>

/*
  core/input.h — Adaptateur entrees pAKAman au-dessus de gb_core.
  UNIQUE proprietaire du bus I2C + ADC : seul input_poll() appelle g_core.pool().
  Le combo loader (RUN+MENU) et la capture (MENU long) reutilisent Keys, ils ne
  relisent PAS l'expander -> pas de lecteurs concurrents sur le bus I2C.
*/
struct Keys {
    uint32_t raw;       // etat brut des touches (bitmask EXPANDER_KEY_*)
    uint32_t pressed;   // fronts appui   (front montant)
    uint32_t released;  // fronts relache (front descendant)

    // Appui maintenu (>500 ms) — conserve pour compat avec keyHeld().
    uint32_t holdStart[16];
    uint32_t held;

    int joxx, joxy;     // joystick centre sur JOYX_MID (echelle ADC historique)

    bool up, down, left, right;
    bool A, B, C, D, RUN, MENU, R1, L1;
};

// Dernier etat lu — certains modules du jeu s'y referent directement.
extern Keys g_keys;

void input_init();
void input_poll(Keys& k);

// Vrai si une des touches du masque est maintenue >= durationMs.
bool keyHeld(const Keys& k, uint32_t keyMask, uint32_t durationMs);
// Vrai apres ~1 s d'appui continu (repere sur les frames).
bool isLongPress(const Keys& k, int key);
