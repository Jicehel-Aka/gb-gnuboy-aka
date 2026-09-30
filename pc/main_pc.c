/**
 * @file main_pc.c
 * @brief Frontend PC (Windows / Linux) : SDL2 au-dessus de gbrt_aka, avec le même lanceur à dossiers que l'AKA.
 *
 * Sans argument : lanceur graphique qui parcourt le dossier "roms" à côté de l'exécutable (sous-dossiers compris).
 * Avec un dossier : le lanceur s'ouvre dans ce dossier. Avec un fichier .gb/.gbc : lance directement la ROM
 * (glisser-déposer sous Windows).
 *
 * usage : gb_gnuboy_pc [--frames N] [--scale N] [--hash] [dossier | rom.gb]
 *   --frames N : s'arrête après N images (tests automatiques)
 *   --scale N  : taille de la fenêtre = 160x144 multiplié par N (défaut 4)
 *   --hash     : affiche un hachage de la dernière image (tests automatiques)
 *
 * Projet  : gb-gnuboy-aka — lecteur Game Boy / Game Boy Color pour la Gamebuino AKA (ESP32-S3).
 * Auteur  : Jicehel (Jicehel-Aka)
 * Licence : voir README.md (cœur gnuboy : GPL v2 (components/gnuboy/COPYING) ; composant gamebuino : LGPL).
 */
#define SDL_MAIN_HANDLED
#include <SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#define MKDIR(p) mkdir((p), 0777)
#endif

#include "gbrt_aka.h"
#include "gbrt_browser.h"
#include "pc_ui.h"

/* État global du frontend PC. */
typedef struct {
    SDL_Window *win;
    SDL_Renderer *ren;
    SDL_Texture *tex;    /* image du jeu 160x144 */
    SDL_Texture *ui_tex; /* lanceur 320x240 */
    SDL_AudioDeviceID audio;
    int quit;            /* fermeture de la fenêtre : on quitte tout */
    int back;            /* Échap en jeu : retour au lanceur */
    int fullscreen;
    uint32_t frames;
    uint64_t hash;
    int trace;
} Pc;

static Pc g;

/* Bascule plein écran (touche F11). */
static void toggle_fullscreen(void) {
    g.fullscreen = !g.fullscreen;
    SDL_SetWindowFullscreen(g.win, g.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}

/* ------------------------------------------------------------------ */
/* Jeu                                                                 */
/* ------------------------------------------------------------------ */

/* Affiche une image RGBA et mémorise son hachage FNV-1a (option --hash). */
static void pc_present(const uint32_t *rgba, int w, int h, void *u) {
    (void)u;
    if (g.tex) {
        SDL_UpdateTexture(g.tex, NULL, rgba, w * (int)sizeof(uint32_t));
        SDL_RenderClear(g.ren);
        SDL_RenderCopy(g.ren, g.tex, NULL, NULL);
        SDL_RenderPresent(g.ren);
    }
    g.frames++;
    uint64_t hsh = 1469598103934665603ull; /* FNV-1a de la dernière image */
    for (int i = 0; i < w * h; i++) { hsh ^= rgba[i]; hsh *= 1099511628211ull; }
    g.hash = hsh;
}

/* Traite les événements SDL (fermeture, Échap, F11) puis traduit le clavier en boutons de la console. */
static uint8_t pc_buttons(void *u) {
    (void)u;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) g.quit = 1;
        if (e.type == SDL_KEYDOWN && !e.key.repeat) {
            if (e.key.keysym.sym == SDLK_ESCAPE) g.back = 1;
            if (e.key.keysym.sym == SDLK_F11 && g.win) toggle_fullscreen();
        }
    }
    const Uint8 *k = SDL_GetKeyboardState(NULL);
    uint8_t b = 0;
    if (k[SDL_SCANCODE_RIGHT] || k[SDL_SCANCODE_D]) b |= GBRT_AKA_BTN_RIGHT;
    if (k[SDL_SCANCODE_LEFT]  || k[SDL_SCANCODE_A]) b |= GBRT_AKA_BTN_LEFT;
    if (k[SDL_SCANCODE_UP]    || k[SDL_SCANCODE_W]) b |= GBRT_AKA_BTN_UP;
    if (k[SDL_SCANCODE_DOWN]  || k[SDL_SCANCODE_S]) b |= GBRT_AKA_BTN_DOWN;
    if (k[SDL_SCANCODE_X] || k[SDL_SCANCODE_SPACE])       b |= GBRT_AKA_BTN_A;
    if (k[SDL_SCANCODE_Z] || k[SDL_SCANCODE_B])           b |= GBRT_AKA_BTN_B;
    if (k[SDL_SCANCODE_RETURN])                           b |= GBRT_AKA_BTN_START;
    if (k[SDL_SCANCODE_BACKSPACE] || k[SDL_SCANCODE_RSHIFT]) b |= GBRT_AKA_BTN_SELECT;
    return b;
}

/* Envoie l'audio à SDL ; jette le surplus au-delà de ~4 images pour garder une latence courte. */
static void pc_audio(const int16_t *lr, size_t frames, void *u) {
    (void)u;
    if (!g.audio) return;
    /* Si plus de ~4 images de son sont déjà en attente, on jette (évite la latence). */
    if (SDL_GetQueuedAudioSize(g.audio) > 4u * 735u * 4u) return;
    SDL_QueueAudio(g.audio, lr, (Uint32)(frames * 2 * sizeof(int16_t)));
}

/* Horloge haute résolution de SDL en microsecondes. */
static uint64_t pc_now_us(void *u) {
    (void)u;
    return (uint64_t)(SDL_GetPerformanceCounter() * 1000000ull / SDL_GetPerformanceFrequency());
}

/* Attente de fin d'image (milliseconde entière ; le reste est rattrapé par l'échéance de la boucle). */
static void pc_sleep_us(uint32_t us, void *u) {
    (void)u;
    if (us >= 1000) SDL_Delay(us / 1000); /* le reliquat est absorbé par l'échéance de la boucle */
}

static bool pc_should_quit(void *u) { (void)u; return g.quit || g.back; }

/* Lance une ROM avec les fonctions SDL ci-dessus ; revient à la fermeture (Échap = retour lanceur). */
static int play(const char *rom, const char *save_dir, uint32_t max_frames) {
    GbrtAkaHal hal;
    memset(&hal, 0, sizeof hal);
    hal.read_buttons = pc_buttons;
    hal.audio_write = g.audio ? pc_audio : NULL;
    hal.time_us = pc_now_us;
    hal.sleep_us = pc_sleep_us;
    hal.should_quit = pc_should_quit;
    hal.present_rgba = pc_present;
    if (g.ren) SDL_RenderSetLogicalSize(g.ren, GBRT_AKA_SCREEN_W, GBRT_AKA_SCREEN_H);
    g.back = 0;
    if (g.audio) SDL_ClearQueuedAudio(g.audio);
    return gbrt_aka_run(&hal, rom, save_dir, max_frames);
}

/* ------------------------------------------------------------------ */
/* Lanceur                                                             */
/* ------------------------------------------------------------------ */

/* Vrai si p est un dossier. */
static int is_dir(const char *p) {
    struct stat st;
    return stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

/* Lanceur graphique : navigation clavier/molette, lancement d'une ROM. Renvoie 0 pour quitter le programme. */
static int launcher(GbrtBrowser *br, const char *save_dir) {
    static uint32_t ui[UI_W * UI_H];
    const int rows = ui_rows();
    char status[64] = "";
    gbrt_browser_move(br, 0, rows);
    if (g.ren) SDL_RenderSetLogicalSize(g.ren, UI_W, UI_H);

    for (;;) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) return 0;
            if (e.type == SDL_MOUSEWHEEL) {
                gbrt_browser_move(br, e.wheel.y > 0 ? -3 : 3, rows);
            }
            if (e.type != SDL_KEYDOWN) continue;
            status[0] = 0;
            switch (e.key.keysym.sym) {
            case SDLK_ESCAPE: return 0;
            case SDLK_F11: toggle_fullscreen(); break;
            case SDLK_DOWN: gbrt_browser_move(br, 1, rows); break;
            case SDLK_UP: gbrt_browser_move(br, -1, rows); break;
            case SDLK_PAGEDOWN: gbrt_browser_move(br, rows, rows); break;
            case SDLK_PAGEUP: gbrt_browser_move(br, -rows, rows); break;
            case SDLK_HOME: br->sel = 0; br->top = 0; break;
            case SDLK_END: br->sel = br->count ? br->count - 1 : 0; gbrt_browser_move(br, 0, rows); break;
            case SDLK_BACKSPACE:
            case SDLK_LEFT:
            case SDLK_z:
                if (gbrt_browser_up(br)) gbrt_browser_move(br, 0, rows);
                break;
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
            case SDLK_RIGHT:
            case SDLK_x:
            case SDLK_SPACE: {
                if (e.key.keysym.sym == SDLK_RIGHT && br->count && !br->entries[br->sel].is_dir) break;
                char rom[GBRT_BROWSER_PATH_MAX];
                int r = gbrt_browser_activate(br, rom, sizeof rom);
                if (r == 0) { gbrt_browser_move(br, 0, rows); break; }
                if (r < 0) break;
                if (g.trace) printf("launch %s\n", rom);
                int rc = play(rom, save_dir, 0);
                if (g.trace) printf("back rc=%d frames=%u\n", rc, g.frames);
                if (g.quit) return 0;
                if (rc != GBRT_AKA_OK) snprintf(status, sizeof status, "Impossible de lancer (code %d)", rc);
                if (g.ren) SDL_RenderSetLogicalSize(g.ren, UI_W, UI_H);
                break;
            }
            default: break;
            }
        }
        ui_draw_browser(ui, br, status, rows);
        if (g.ren && g.ui_tex) {
            SDL_UpdateTexture(g.ui_tex, NULL, ui, UI_W * (int)sizeof(uint32_t));
            SDL_RenderClear(g.ren);
            SDL_RenderCopy(g.ren, g.ui_tex, NULL, NULL);
            SDL_RenderPresent(g.ren);
        }
        SDL_Delay(16);
    }
}

/* Dossier "roms" à côté de l'exécutable (créé s'il manque pour indiquer où déposer les ROM). */
static void default_roms_dir(char *out, size_t cap) {
    char *base = SDL_GetBasePath();
    snprintf(out, cap, "%sroms", base ? base : "./");
    SDL_free(base);
    if (!is_dir(out)) {
        MKDIR(out); /* crée le dossier pour que l'utilisateur sache où mettre ses ROM */
        if (!is_dir(out)) snprintf(out, cap, "%s", ".");
    }
}

/* Analyse des options, initialisation SDL (fenêtre, audio), puis ROM directe ou lanceur. */
int main(int argc, char **argv) {
    const char *target = NULL;
    uint32_t max_frames = 0;
    int scale = 4, want_hash = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--frames") && i + 1 < argc) max_frames = (uint32_t)atoi(argv[++i]);
        else if (!strcmp(argv[i], "--scale") && i + 1 < argc) scale = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--hash")) want_hash = 1;
        else target = argv[i];
    }
    if (scale < 1) scale = 1;
    g.trace = getenv("GBRT_PC_TRACE") != NULL;

    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "SDL_Init : %s\n", SDL_GetError());
        return 1;
    }
    g.win = SDL_CreateWindow("Game Boy (gnuboy)", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                             GBRT_AKA_SCREEN_W * scale, GBRT_AKA_SCREEN_H * scale, SDL_WINDOW_RESIZABLE);
    if (g.win) g.ren = SDL_CreateRenderer(g.win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (g.win && !g.ren) g.ren = SDL_CreateRenderer(g.win, -1, SDL_RENDERER_SOFTWARE);
    if (g.ren) {
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
        g.tex = SDL_CreateTexture(g.ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                  GBRT_AKA_SCREEN_W, GBRT_AKA_SCREEN_H);
        g.ui_tex = SDL_CreateTexture(g.ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, UI_W, UI_H);
    }
    if (!g.tex) fprintf(stderr, "Affichage indisponible : %s (exécution sans image)\n", SDL_GetError());

    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = GBRT_AKA_AUDIO_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    g.audio = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (g.audio) SDL_PauseAudioDevice(g.audio, 0);
    else fprintf(stderr, "Audio indisponible : %s\n", SDL_GetError());

    char *pref = SDL_GetPrefPath("gb-gnuboy", "aka"); /* crée le dossier des sauvegardes */
    if (pref) {
        size_t n = strlen(pref);
        while (n && (pref[n - 1] == '/' || pref[n - 1] == '\\')) pref[--n] = 0;
    }

    int rc = 0;
    if (target && !is_dir(target)) {
        /* ROM donnée directement : pas de lanceur. */
        rc = play(target, pref, max_frames);
        if (rc != GBRT_AKA_OK) fprintf(stderr, "Échec (code %d) : impossible de lancer %s\n", rc, target);
        if (want_hash) printf("frames=%u hash=%016llx\n", g.frames, (unsigned long long)g.hash);
        rc = rc == GBRT_AKA_OK ? 0 : 1;
    } else {
        char root[GBRT_BROWSER_PATH_MAX];
        if (target) snprintf(root, sizeof root, "%s", target);
        else default_roms_dir(root, sizeof root);
        GbrtBrowser br;
        if (!gbrt_browser_open(&br, root)) {
            fprintf(stderr, "Dossier illisible : %s\n", root);
            rc = 1;
        } else {
            launcher(&br, pref);
            gbrt_browser_close(&br);
        }
    }

    if (g.audio) SDL_CloseAudioDevice(g.audio);
    if (g.ui_tex) SDL_DestroyTexture(g.ui_tex);
    if (g.tex) SDL_DestroyTexture(g.tex);
    if (g.ren) SDL_DestroyRenderer(g.ren);
    if (g.win) SDL_DestroyWindow(g.win);
    SDL_free(pref);
    SDL_Quit();
    return rc;
}
