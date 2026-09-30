/*
  core/input.cpp — Adaptateur entrees pAKAman au-dessus de gb_core.
  Seul input_poll() appelle g_core.pool() : c'est l'unique lecteur du bus I2C
  et de l'ADC (joystick). Toutes les autres briques (combo loader, capture,
  menu, jeu) travaillent sur la structure Keys deja remplie ici.
*/
#include "core/input.h"
#include "gb_core.h"
#include "gb_ll_common.h"   // EXPANDER_KEY_*, JOYX_MID
#include "esp_timer.h"

extern gb_core g_core;       // instance unique, definie dans app_main

Keys g_keys;

void input_init() {
    // L'init materiel (ecran, bus, peripheriques) est faite par g_core.init().
    for (int i = 0; i < 16; ++i) g_keys.holdStart[i] = 0;
    g_keys.held = 0;
}

void input_poll(Keys& k) {
    g_core.pool();                              // lit boutons + joystick (1 seule fois)

    const uint16_t raw = g_core.buttons.state();
    k.raw      = raw;
    k.pressed  = g_core.buttons.pressed();      // fronts fournis par le SDK
    k.released = g_core.buttons.released();

    // --- Appui maintenu (timestamps par bit) ---
    const uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
    k.held = 0;
    for (int bit = 0; bit < 16; ++bit) {
        const uint32_t mask = (1u << bit);
        if (raw & mask) {
            if (k.holdStart[bit] == 0) k.holdStart[bit] = now;
            if (now - k.holdStart[bit] >= 500) k.held |= mask;
        } else {
            k.holdStart[bit] = 0;
        }
    }

    k.up    = raw & EXPANDER_KEY_UP;
    k.down  = raw & EXPANDER_KEY_DOWN;
    k.left  = raw & EXPANDER_KEY_LEFT;
    k.right = raw & EXPANDER_KEY_RIGHT;
    k.A     = raw & EXPANDER_KEY_A;
    k.B     = raw & EXPANDER_KEY_B;
    k.C     = raw & EXPANDER_KEY_C;
    k.D     = raw & EXPANDER_KEY_D;
    k.RUN   = raw & EXPANDER_KEY_RUN;
    k.MENU  = raw & EXPANDER_KEY_MENU;
    k.R1    = raw & EXPANDER_KEY_R1;
    k.L1    = raw & EXPANDER_KEY_L1;

    // get_x()/get_y() renvoient -1000..1000 (centre calibre). Le jeu attend une
    // valeur centree sur JOYX_MID (echelle ADC historique) : on reconvertit,
    // ainsi pacman.cpp (comparaisons JOYX_MID +/- DEADZONE) reste inchange.
    // NB : si HAUT/BAS te paraissent inverses sur ta carte, inverse le signe de
    //      get_y() ci-dessous (une seule ligne a changer).
    k.joxx = JOYX_MID + (int)((long)g_core.joystick.get_x() * JOYX_MID / 1000);
    k.joxy = JOYX_MID + (int)((long)g_core.joystick.get_y() * JOYX_MID / 1000);

    g_keys = k;                                 // expose le dernier etat lu
}

bool keyHeld(const Keys& k, uint32_t keyMask, uint32_t durationMs) {
    const uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
    for (int bit = 0; bit < 16; ++bit) {
        if (keyMask & (1u << bit)) {
            if (k.holdStart[bit] != 0 && (now - k.holdStart[bit] >= durationMs))
                return true;
        }
    }
    return false;
}

bool isLongPress(const Keys& k, int key) {
    static int pressDuration = 0;
    if (k.raw & key) {
        if (++pressDuration > 60) { pressDuration = 0; return true; }
    } else {
        pressDuration = 0;
    }
    return false;
}
