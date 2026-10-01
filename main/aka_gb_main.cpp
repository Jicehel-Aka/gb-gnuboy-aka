/**
 * @file aka_gb_main.cpp
 * @brief Lanceur Game Boy pour Gamebuino AKA (ESP-IDF) : sélecteur de ROM, affichage, son, boutons.
 *
 * Au-dessus du composant "gamebuino" (gb_core, gb_graphics, gb_audio_player) et de la couche gbrt_aka.
 *
 * - Sélecteur de ROM : navigue dans /sdcard/GB/ et ses sous-dossiers (*.gb, *.gbc)
 * - Image : RGB555 du PPU convertie directement dans `framebuffer` (BGR565 320x240), centrée en 1x
 *   ou agrandie 1,5x (L1 pour basculer), puis gb_graphics::update(). Saut d'affichage adaptatif si retard.
 * - Son : le PCM du runtime (stéréo 44100 Hz) est mixé en mono et servi au gb_audio_player par une « piste » à tampon circulaire.
 * - Sauvegardes : /sdcard/Gnuboy_MK/saves/<nom_rom>.sav (RAM batterie + horloge, format gnuboy)
 *
 * Commandes en jeu : croix / joystick = croix, A = A, B = B, RUN = Start, C = Select,
 *                    L1 = zoom 1x / 1,5x, MENU court = menu système AKA (Reprendre, Choisir un jeu, Commandes, Langue, Volume, Crédits,
 *                    Retour au loader ; l'émulation est en pause tant qu'il est ouvert), MENU long (500 ms) = capture d'écran (BMP),
 *                    RUN + MENU (500 ms) = retour au loader (la RAM batterie est écrite avant).
 * Dans le sélecteur : haut/bas = choisir, A = lancer ou ouvrir un dossier, B = dossier parent,
 *                     L1/R1 = page précédente/suivante, MENU (500 ms) = retour au loader.
 *
 * Projet  : gb-gnuboy-aka — lecteur Game Boy / Game Boy Color pour la Gamebuino AKA (ESP32-S3).
 * Auteur  : Jicehel (Jicehel-Aka)
 * Licence : voir README.md (cœur gnuboy : GPL v2 (components/gnuboy/COPYING) ; composant gamebuino : LGPL).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <string>

#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* gb_err.h n'existe que dans le composant gamebuino récent. L'ancien (sans set_run_power_off) éteint la console à CHAQUE appui sur RUN,
 * or RUN sert ici de Start : refuser de compiler vaut mieux qu'une console qui s'éteint en plein jeu. */
#if !__has_include("gb_err.h")
#error "composant gamebuino trop ancien : il faut la version avec gb_err.h et gb_buttons::set_run_power_off() (ex. celle de pAKAman / AKA-Love). Avec l'ancienne, RUN (= Start) éteindrait la console."
#endif
#include "gb_audio_player.h"
#include "gb_common.h"
#include "gb_core.h"
#include "gb_err.h"
#include "gb_graphics.h"
#include "gbrt_aka.h"
#include "gbrt_browser.h"
#include "aka_runtime/aka_runtime.h" /* menu système AKA : pause/reprise, volume, langue, capture (MENU long), retour au loader (RUN+MENU) */
#include "core/input.h"

#define GB_DIR   MOUNT_POINT "/GB"
#define SAVE_DIR MOUNT_POINT "/Gnuboy_MK/saves" /* hors de /GB (le sélecteur ne le montre pas) et distinct des sauvegardes de l'autre émulateur */

static_assert(sizeof(gb_pixel) == 2, "USE_VIDEO_256_INDEXED n'est pas supporté par ce lecteur (BGR565 requis)");

/* ------------------------------------------------------------------ */
/* Matériel                                                            */
/* ------------------------------------------------------------------ */

/* Instances globales (non statiques) : aka_runtime et core/input.cpp les réclament sous ces noms (`extern gb_core g_core`, `extern gb_graphics gfx`). */
gb_core g_core;       /* boutons, joystick, poll du matériel */
gb_graphics gfx;      /* écran : `framebuffer` + update() */
static gb_audio_player *g_audio; /* mixeur audio du composant gamebuino */

/* ------------------------------------------------------------------ */
/* Piste audio : tampon circulaire mono, alimenté par l'émulateur      */
/* ------------------------------------------------------------------ */

/* Piste du mixeur alimentée par l'émulateur : push_stereo() écrit (mixage mono L+R)/2), play_callback() lit à la demande du mixeur.
 * Si le tampon est plein on jette le surplus ; s'il est vide on renvoie GB_ERR (le mixeur ignore alors la piste ; GB_OK = 0 = tampon rempli, contrat de gb_audio_player.cpp). */
class GbStreamTrack : public gb_audio_track_base {
public:
    void push_stereo(const int16_t *lr, size_t frames) {
        for (size_t i = 0; i < frames; ++i) {
            if (count_ >= kSize) return; /* plein : on jette le surplus */
            int32_t m = ((int32_t)lr[2 * i] + (int32_t)lr[2 * i + 1]) / 2;
            m = (m * gain_) / 256; /* volume du menu : 256 = niveau d'origine */
            if (m > 32767) m = 32767;
            if (m < -32768) m = -32768;
            ring_[head_] = (int16_t)m;
            head_ = (head_ + 1) % kSize;
            ++count_;
        }
    }
    void reset() { head_ = tail_ = count_ = 0; }
    /* Volume du menu (0..100) -> gain ; 80 (valeur par défaut du menu AKA) = niveau d'origine, 100 = x1,25 (saturé à ±32767). */
    void set_volume(uint8_t v) { gain_ = (int32_t)v * 256 / 80; }

    int play_callback(int16_t *out, uint16_t n) override {
        if (count_ == 0) return GB_ERR; /* rien à jouer : le mixeur n'envoie pas de tampon */
        for (uint16_t i = 0; i < n; ++i) {
            if (count_) {
                out[i] = ring_[tail_];
                tail_ = (tail_ + 1) % kSize;
                --count_;
            } else {
                out[i] = 0;
            }
        }
        return GB_OK;
    }
    void stop_playing() override { reset(); }
    bool is_playing() override { return count_ > 0; }

private:
    static const size_t kSize = 8192;
    int16_t ring_[kSize];
    size_t head_ = 0, tail_ = 0, count_ = 0;
    int32_t gain_ = 256;
};

static GbStreamTrack g_track;

/* ------------------------------------------------------------------ */
/* Lancement d'un jeu                                                  */
/* ------------------------------------------------------------------ */

static bool g_zoom = false;      /* false = 1x centré, true = 1,5x (bascule avec L1) */
static bool g_clear_pending = true; /* true : effacer l'écran avant la prochaine image (changement de zoom, nouveau jeu) */
static bool g_change_game = false;   /* mis par l'entrée « Choisir un jeu » du menu : quitte la partie vers le sélecteur */
static Keys g_menu_keys;             /* dernier état des touches, pour aka_runtime (input_poll est le seul lecteur du bus I2C) */

/* Chemin rapide : l'image RGB555 du PPU est convertie en BGR565 directement dans `framebuffer`
 * (aucun tampon intermédiaire), centrée en 1x ou agrandie 1,5x. */
static void gb_present_rgb555(const uint16_t *px, int w, int h, void *) { /* appelé une fois par image affichée */
    if (g_clear_pending) {
        memset(framebuffer, 0, sizeof(gb_pixel) * SCREEN_WIDTH * SCREEN_HEIGHT);
        g_clear_pending = false;
    }
    if (!g_zoom) {
        const int ox = (SCREEN_WIDTH - w) / 2, oy = (SCREEN_HEIGHT - h) / 2;
        for (int y = 0; y < h; ++y)
            gbrt_aka_rgb555_to_bgr565(px + (size_t)y * w, &framebuffer[(size_t)(oy + y) * SCREEN_WIDTH + ox], (size_t)w);
    } else {
        const int dw = w * 3 / 2, dh = h * 3 / 2;
        const int ox = (SCREEN_WIDTH - dw) / 2, oy = (SCREEN_HEIGHT - dh) / 2;
        static uint16_t line[GBRT_AKA_SCREEN_W];
        int last_sy = -1;
        for (int y = 0; y < dh; ++y) {
            int sy = y * 2 / 3;
            if (sy != last_sy) {
                gbrt_aka_rgb555_to_bgr565(px + (size_t)sy * w, line, (size_t)w);
                last_sy = sy;
            }
            gb_pixel *dst = &framebuffer[(size_t)(oy + y) * SCREEN_WIDTH + ox];
            for (int x = 0; x < dw; ++x) dst[x] = line[x * 2 / 3];
        }
    }
    gfx.update();
}

/* Toutes les 120 images, sur la liaison série : vitesse et charge. Charge > 100 % = trop lent. */
static void gb_report(float fps, int load, uint32_t skipped, void *) {
    printf("[gb] %.1f img/s, charge %d %%, affichages sautes %u\n", fps, load, (unsigned)skipped);
}

/* Menu système AKA, bloquant : tant qu'il est ouvert l'émulation est figée (= pause) et le son se tait.
 * La cadence se recale toute seule à la sortie (retard > 2 images => nouvelle échéance). */
static void run_system_menu() {
    g_track.reset();
    do {
        vTaskDelay(pdMS_TO_TICKS(16));
        input_poll(g_menu_keys);
        akaRuntime.update(g_menu_keys);
    } while (akaRuntime.isMenuOpen());
    /* Laisse relâcher la touche qui a validé (A, C, MENU...) : sinon le jeu la verrait comme un appui. */
    for (int i = 0; i < 30; ++i) {
        g_core.pool();
        if (g_core.buttons.state() == 0) break;
        vTaskDelay(pdMS_TO_TICKS(16));
    }
    g_clear_pending = true; /* efface les restes de la boîte du menu */
    g_track.reset();
}

/* Lit les boutons (input_poll() : seul lecteur du bus I2C/ADC), laisse aka_runtime traiter RUN+MENU (loader), MENU long (capture)
 * et MENU court (menu système), puis convertit en masque GBRT_AKA_BTN_* ; L1 bascule le zoom. C = Select, RUN = Start. */
static uint8_t gb_read_buttons(void *) {
    input_poll(g_menu_keys);
    akaRuntime.update(g_menu_keys);            /* MENU court : ouvre le menu (dessiné au tour suivant) */
    if (akaRuntime.isMenuOpen()) {
        run_system_menu();
        return 0;                              /* pas d'appui sur cette image */
    }
    const uint16_t k = (uint16_t)(g_core.buttons.state() | g_core.joystick.state());
    if (g_core.buttons.pressed(gb_buttons::KEY_L1)) {
        g_zoom = !g_zoom;
        g_clear_pending = true;
    }
    uint8_t b = 0;
    if (k & gb_buttons::KEY_RIGHT) b |= GBRT_AKA_BTN_RIGHT;
    if (k & gb_buttons::KEY_LEFT)  b |= GBRT_AKA_BTN_LEFT;
    if (k & gb_buttons::KEY_UP)    b |= GBRT_AKA_BTN_UP;
    if (k & gb_buttons::KEY_DOWN)  b |= GBRT_AKA_BTN_DOWN;
    if (k & gb_buttons::KEY_A)     b |= GBRT_AKA_BTN_A;
    if (k & gb_buttons::KEY_B)     b |= GBRT_AKA_BTN_B;
    if (k & gb_buttons::KEY_C)     b |= GBRT_AKA_BTN_SELECT;
    if (k & gb_buttons::KEY_RUN)   b |= GBRT_AKA_BTN_START;
    return b;
}

/* Reçoit l'audio d'une image (stéréo 44,1 kHz), l'empile dans la piste puis laisse le mixeur consommer. */
static void gb_audio_write(const int16_t *lr, size_t frames, void *) {
    g_track.push_stereo(lr, frames);
    g_audio->pool();
}

/* Horloge microseconde (esp_timer, précise au µs). */
static uint64_t gb_now_us(void *) { return (uint64_t)esp_timer_get_time(); }

/* Attente de fin d'image. vTaskDelay() n'a que la résolution du tick FreeRTOS (10 ms si CONFIG_FREERTOS_HZ=100,
 * valeur par défaut d'ESP-IDF) : avec pdMS_TO_TICKS(us / 1000) une attente de 9 ms tombait à 0 tick, donc pas
 * d'attente du tout, et l'émulation pouvait tourner plus vite que la console d'origine. On dort donc par ticks entiers
 * tant qu'il reste plus d'un tick, puis on cède le processeur (taskYIELD) jusqu'à l'échéance exacte mesurée par esp_timer. */
static void gb_sleep_us(uint32_t us, void *) {
    const uint64_t end = (uint64_t)esp_timer_get_time() + us;
    const uint32_t tick_us = (uint32_t)(portTICK_PERIOD_MS * 1000);
    for (;;) {
        const uint64_t now = (uint64_t)esp_timer_get_time();
        if (now >= end) break;
        if (end - now > tick_us) vTaskDelay(1);
        else taskYIELD();
    }
}

/* Vrai quand le menu système a demandé « Choisir un jeu » : la partie se ferme (RAM batterie écrite) et le sélecteur revient.
 * RUN+MENU (retour au loader) est géré par aka_runtime, qui écrit d'abord la sauvegarde via gbrt_aka_flush_save(). */
static bool gb_should_quit(void *) { return g_change_game; }

/* Callbacks du menu système. */
static void on_choose_game() { g_change_game = true; }
static void on_volume(uint8_t music, uint8_t) { g_track.set_volume(music); } /* une seule piste : seul « Musique » règle le volume */
static void on_before_exit() { gbrt_aka_flush_save(); }

/* ------------------------------------------------------------------ */
/* Sélecteur de ROM                                                    */
/* ------------------------------------------------------------------ */

/* Texte à la position (x, y) dans la couleur donnée. */
static void draw_text(int x, int y, const char *s, uint16_t color) {
    gfx.setColor(color);
    gfx.move_cursor((uint16_t)x, (uint16_t)y);
    gfx.print_str(s);
}

/* Affiche un message sur 1 ou 2 lignes pendant `ms` millisecondes (erreurs de lancement, dossier absent). */
static void show_message(const char *l1, const char *l2, uint32_t ms) {
    gfx.clear(color_black);
    draw_text(16, 100, l1, color_yellow);
    if (l2) draw_text(16, 116, l2, color_white);
    gfx.update();
    vTaskDelay(pdMS_TO_TICKS(ms));
}

/* Redémarre sur la partition OTA_1 qui contient le loader de la console (ne revient pas si elle existe). */
static void return_to_loader() {
    const esp_partition_t *loader =
        esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, nullptr);
    if (loader) {
        esp_ota_set_boot_partition(loader);
        esp_restart();
    }
}

/* Renvoie true et remplit `path` si une ROM est choisie, false pour quitter vers le loader. */
/* Sélecteur de ROM, affiché à 60 Hz. Haut/bas = choisir, L1/R1 = page, A = ouvrir/lancer, B = dossier parent. */
static bool pick_rom(GbrtBrowser *br, std::string &path) {
    const int rows = 17;
    uint32_t menu_since = 0;
    gbrt_browser_move(br, 0, rows);
    for (;;) {
        g_core.pool();
        const bool down = g_core.buttons.pressed(gb_buttons::KEY_DOWN) || g_core.joystick.pressed(gb_buttons::KEY_DOWN);
        const bool up = g_core.buttons.pressed(gb_buttons::KEY_UP) || g_core.joystick.pressed(gb_buttons::KEY_UP);
        if (down) gbrt_browser_move(br, 1, rows);
        if (up) gbrt_browser_move(br, -1, rows);
        if (g_core.buttons.pressed(gb_buttons::KEY_L1)) gbrt_browser_move(br, -rows, rows);
        if (g_core.buttons.pressed(gb_buttons::KEY_R1)) gbrt_browser_move(br, rows, rows);
        if (g_core.buttons.pressed(gb_buttons::KEY_A)) {
            char full[GBRT_BROWSER_PATH_MAX];
            int r = gbrt_browser_activate(br, full, sizeof(full));
            if (r == 1) { path = full; return true; }
            gbrt_browser_move(br, 0, rows);
        }
        if (g_core.buttons.pressed(gb_buttons::KEY_B) && gbrt_browser_up(br))
            gbrt_browser_move(br, 0, rows);

        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
        if (g_core.buttons.state() & gb_buttons::KEY_MENU) {
            if (!menu_since) menu_since = now;
            if (now - menu_since >= 500) return false;
        } else {
            menu_since = 0;
        }

        gfx.clear(color_black);
        char head[64];
        snprintf(head, sizeof(head), "Game Boy  %.40s", gbrt_browser_rel(br));
        draw_text(8, 6, head, color_yellow);
        if (br->count == 0) {
            draw_text(8, 40, "Dossier vide", color_white);
            draw_text(8, 56, "B : dossier parent", color_gray);
        }
        for (int i = 0; i < rows && br->top + i < br->count; ++i) {
            const int idx = br->top + i;
            const GbrtEntry &e = br->entries[idx];
            char line[64];
            snprintf(line, sizeof(line), "%c %.38s%s", (idx == br->sel) ? '>' : ' ', e.name, e.is_dir ? "/" : "");
            uint16_t col = (idx == br->sel) ? color_white : (e.is_dir ? color_lightblue : color_gray);
            draw_text(8, 26 + i * 11, line, col);
        }
        if (br->truncated) draw_text(8, 214, "Liste tronquee (1024 max)", color_orange);
        draw_text(8, 228, "A:jouer/ouvrir B:retour MENU long:loader", color_darkgray);
        gfx.update();
        vTaskDelay(pdMS_TO_TICKS(16));
    }
}

/* ------------------------------------------------------------------ */

/* Point d'entrée ESP-IDF : initialise la console, crée /GB et /Gnuboy_MK/saves, puis boucle sélecteur -> jeu -> sélecteur. */
extern "C" void app_main(void) {
    static gb_audio_player audio;
    g_audio = &audio;

    if (g_core.init() != GB_OK) { /* échec critique (I2C, ADC, expander, ampli) : le LCD n'est pas initialisé, on ne peut qu'écrire sur la liaison série */
        printf("[gb] gb_core::init() a echoue : arret\n");
        return;
    }
    g_core.buttons.set_run_power_off(false); /* RUN = Start : ne doit JAMAIS éteindre la console (seul RUN+MENU 500 ms quitte le jeu) */
    gfx.set_refresh_rate(60); /* ~59,73 Hz natif : évite le battement avec le vsync 70/35 Hz */
    audio.add_track(&g_track, 1.0f);
    mkdir(GB_DIR, 0777);

    /* Menu système AKA (MENU court), capture (MENU long) et retour au loader (RUN+MENU). Les textes viennent de
     * /sdcard/AKA/lang/<langue>.json et /sdcard/Gnuboy_MK/lang/<langue>.json (fournis dans SD_files/). */
    akaRuntime.begin("Gnuboy_MK");
    static const char *const kControls[] = {"CTRL_DPAD", "CTRL_AB", "CTRL_START", "CTRL_SELECT", "CTRL_ZOOM", "CTRL_MENU", "CTRL_SHOT", "CTRL_LOADER", nullptr};
    akaRuntime.setControlsKeys(kControls);
    akaRuntime.setCredits("Game Boy (gnuboy)", "Jicehel / gnuboy", "GPL v2 (coeur gnuboy)", "github.com/ducalex/retro-go");
    akaRuntime.setGameMenuCallback(on_choose_game);
    akaRuntime.setVolumeChangedCallback(on_volume);
    akaRuntime.setBeforeExitCallback(on_before_exit);
    g_track.set_volume(akaRuntime.getMusicVolume());
    mkdir(SAVE_DIR, 0777); /* après begin() : il crée /sdcard/Gnuboy_MK, dossier parent des sauvegardes */

    GbrtBrowser browser;
    if (!gbrt_browser_open(&browser, GB_DIR)) {
        show_message("Dossier /GB introuvable", "Cree-le sur la carte SD", 3000);
        return_to_loader();
        return;
    }

    for (;;) {
        std::string path;
        if (!pick_rom(&browser, path)) {
            return_to_loader();
            continue;
        }
        g_clear_pending = true;
        g_change_game = false;
        g_track.reset();

        GbrtAkaHal hal = {};
        hal.read_buttons = gb_read_buttons;
        hal.audio_write = gb_audio_write;
        hal.time_us = gb_now_us;
        hal.sleep_us = gb_sleep_us;
        hal.should_quit = gb_should_quit;
        hal.present_rgb555 = gb_present_rgb555;
        hal.auto_frameskip = true;
        hal.report = gb_report;

        int rc = gbrt_aka_run(&hal, path.c_str(), SAVE_DIR, 0);
        if (rc != GBRT_AKA_OK) {
            show_message("Impossible de lancer la ROM", gbrt_aka_strerror(rc), 2500);
        }
        /* Attend le relâchement des touches (2 s max) : sinon une touche encore tenue agirait dans le sélecteur. */
        for (int i = 0; i < 125; ++i) {
            g_core.pool();
            if (g_core.buttons.state() == 0) break;
            vTaskDelay(pdMS_TO_TICKS(16));
        }
    }
}
