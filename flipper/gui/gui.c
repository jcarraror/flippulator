#include "gui_i.h"

#include <furi.h>
#include <pthread.h>
#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
#endif
#include <SDL2/SDL.h>
#if defined(__has_include)
    #if __has_include(<SDL2/SDL_ttf.h>)
        #include <SDL2/SDL_ttf.h>
        #define FLIPPULATOR_HAS_SDL_TTF 1
    #endif
#endif
#ifndef FLIPPULATOR_HAS_SDL_TTF
    #define FLIPPULATOR_HAS_SDL_TTF 0
#endif
#include <math.h>
#include <flippulator_defines.h>
#include <termios.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define FLIPPULATOR_FONT_SIZE 16

#ifndef FLIPPULATOR_APP_NAME
#define FLIPPULATOR_APP_NAME "flippulator"
#endif

#define SCR_SCALE   5               /* host pixels per Flipper pixel          */
#define SCR_W       (128 * SCR_SCALE)  /* 640 */
#define SCR_H       (64  * SCR_SCALE)  /* 320 */
#define BZL_L       20              /* bezel left                             */
#define BZL_R       20              /* bezel right                            */
#define BZL_T       28              /* bezel top                              */
#define BZL_B       20              /* bezel bottom                           */
#define DEV_W       (SCR_W + BZL_L + BZL_R)   /* 680  */
#define DEV_H       (SCR_H + BZL_T + BZL_B)   /* 368  */
#define HUD_H       122             /* debug HUD below device                 */
#define WIN_W       DEV_W           /* 680  */
#define WIN_H       (DEV_H + HUD_H) /* 490  */

extern bool global_vibro_on;
extern float global_sound_freq;
extern float global_sound_volume;
extern uint8_t global_led[3];
extern uint8_t global_backlight_brightness;

extern struct termios global_old_tio;

static SDL_Renderer* renderer;
static SDL_Window* window;
static SDL_Rect rect;
static SDL_Event event;
#if FLIPPULATOR_HAS_SDL_TTF
static TTF_Font* HaxrCorp4089;
static const SDL_Color Black = {0x00, 0x00, 0x00, 0xff};
#endif
static SDL_AudioDeviceID audio_device;
static SDL_AudioSpec audio_spec;
static bool running = true;
static bool show_debug_grid = false;
static bool show_host_hud = true;
static float s_time = 0;

#include <stdio.h>
#include <termios.h>

Gui* gui_alloc() {
    Gui* gui = calloc(1, sizeof(Gui));
    gui->canvas = canvas_init();
    return gui;
}

void gui_free(Gui* gui) {
    if(gui == NULL) {
        return;
    }

    if(gui->view_port != NULL) {
        gui_remove_view_port(gui, gui->view_port);
    }

    if(gui->canvas != NULL) {
        canvas_free(gui->canvas);
        gui->canvas = NULL;
    }

    free(gui);
}

void exit_sdl(uint8_t code) {
    if(furi_record_status()) {
        Gui* gui = furi_record_open(RECORD_GUI);
        if(gui != NULL) {
            gui_free(gui);
        }
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &global_old_tio);
    exit(code);
}

static float sine(void) {
    float to_ret = sin(s_time);

    s_time += global_sound_freq * M_PI * 2 / AUDIO_FREQUENCY;
    if(s_time >= M_PI * 2)
        s_time -= M_PI * 2;
    
    return to_ret;
}

static void sound_cb(void* ctx, uint8_t* stream, int len) {
    UNUSED(ctx);
    uint16_t* snd = (uint16_t*)stream;
    uint16_t vol_l = (global_sound_volume / 60.0) * 32767;
    len /= sizeof(*snd);
    for(int i = 0; i < len; i++) {
        if(global_sound_freq == 0) {
            snd[i] = 0;
            continue;
        }
        if(AUDIO_WAVE_TYPE) {
            snd[i] = sine() > 0 ? vol_l : -vol_l;
        } else {
            snd[i] = vol_l * sine();
        }
    }
}

// 6 buttons

// 0 Up
// 1 Down
// 2 Right
// 3 Left
// 4 OK
// 5 Back

#define BUTTONS_COUNT 6
// Not a define because it's only used
// in this file

static bool held_down[BUTTONS_COUNT];
static uint64_t held_time[BUTTONS_COUNT];
static InputKey key_map[] = {
    InputKeyUp, InputKeyDown, InputKeyRight,
    InputKeyLeft, InputKeyOk, InputKeyBack
};

static void* input_loop(void* _view_port) {
    ViewPort* view_port = _view_port;
    while(running) {
        for(uint8_t i = 0; i < BUTTONS_COUNT; i++) {
            if(!held_down[i]) continue;
            if(held_time[i] % INPUT_PRESS_TICKS == 0 && held_time[i] != 0) {
                if(view_port->input_callback == NULL) {
                    held_time[i]++;
                    continue;
                }

                InputEvent e = {0};
                e.key = key_map[i];
                uint32_t presses = held_time[i] / INPUT_PRESS_TICKS;
                if(presses < INPUT_LONG_PRESS_COUNTS && presses > 0)
                    e.type = InputTypeShort;
                else if(presses == INPUT_LONG_PRESS_COUNTS)
                    e.type = InputTypeLong;
                else if(presses != 0)
                    e.type = InputTypeRepeat;
                view_port->input_callback(&e, view_port->input_callback_context);
            }
            held_time[i]++;
        }
        furi_delay_tick(1);
    }
    return NULL;
}

static void* handle_input(void* _view_port) {
    ViewPort* view_port = _view_port;
    while(running) {
        while(SDL_PollEvent(&event)) {
            if(event.type == SDL_QUIT) exit_sdl(0);
            if(event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
                if(event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_g) {
                    show_debug_grid = !show_debug_grid;
                    continue;
                }

                if(event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_h) {
                    show_host_hud = !show_host_hud;
                    continue;
                }

                int8_t key = -1;
                if(event.key.keysym.sym == SDLK_UP) key = 0;
                else if(event.key.keysym.sym == SDLK_DOWN) key = 1;
                else if(event.key.keysym.sym == SDLK_LEFT) key = 3;
                else if(event.key.keysym.sym == SDLK_RIGHT) key = 2;
                else if(event.key.keysym.sym == SDLK_z) key = 4;
                else if(event.key.keysym.sym == SDLK_x) key = 5;

                if(key < 0) {
                    continue;
                }

                if(!held_down[(uint8_t)key] || event.type == SDL_KEYUP) {
                    held_time[(uint8_t)key] = 0;
                    held_down[(uint8_t)key] = event.type == SDL_KEYDOWN;

                    if(view_port->input_callback != NULL) {
                        InputEvent e = {
                            .type = event.type == SDL_KEYDOWN ? InputTypePress : InputTypeRelease,
                            .key = key_map[(uint8_t)key],
                        };
                        view_port->input_callback(&e, view_port->input_callback_context);
                    }
                }
            }
        }
    }
    return NULL;
}

static void renderMessage(const char* msg, int x, int y) {
#if FLIPPULATOR_HAS_SDL_TTF
    if(HaxrCorp4089 == NULL || msg == NULL) {
        return;
    }

    SDL_Surface* surfaceMessage = TTF_RenderText_Solid(HaxrCorp4089, msg, Black);
    if(surfaceMessage == NULL) {
        return;
    }

    SDL_Texture* message = SDL_CreateTextureFromSurface(renderer, surfaceMessage);
    SDL_Rect message_rect = {x, y, 0, 0};
    if(message == NULL) {
        SDL_FreeSurface(surfaceMessage);
        return;
    }

    TTF_SizeText(HaxrCorp4089, msg, &message_rect.w, &message_rect.h);

    message_rect.w *= 2;
    message_rect.h *= 2;

    SDL_RenderCopy(renderer, message, NULL, &message_rect);

    SDL_FreeSurface(surfaceMessage);
    SDL_DestroyTexture(message);
#else
    UNUSED(msg);
    UNUSED(x);
    UNUSED(y);
#endif
}

static void render_screen_bg(void) {
    const float bl = global_backlight_brightness / 255.0f;
    const uint8_t r = (uint8_t)(0x30 + bl * (0xff - 0x30));
    const uint8_t g = (uint8_t)(0x14 + bl * (0x88 - 0x14));
    SDL_SetRenderDrawColor(renderer, r, g, 0x00, 0xff);
    SDL_Rect sr = {BZL_L, BZL_T, SCR_W, SCR_H};
    SDL_RenderFillRect(renderer, &sr);
}

static void render_led_indicator(void) {
    SDL_SetRenderDrawColor(renderer, global_led[0], global_led[1], global_led[2], 0xff);
    SDL_Rect led = {BZL_L + SCR_W - 14, (BZL_T - 8) / 2, 8, 8};
    SDL_RenderFillRect(renderer, &led);
    SDL_SetRenderDrawColor(renderer, 0x55, 0x55, 0x55, 0xff);
    SDL_RenderDrawRect(renderer, &led);
}

static void render_debug_grid(void) {
    if(!show_debug_grid) {
        return;
    }

    SDL_SetRenderDrawColor(renderer, 0xd8, 0x94, 0x38, 0xff);
    for(int x = 0; x <= 128; x += 8) {
        SDL_RenderDrawLine(renderer, BZL_L + x * SCR_SCALE, BZL_T, BZL_L + x * SCR_SCALE, BZL_T + SCR_H);
    }
    for(int y = 0; y <= 64; y += 8) {
        SDL_RenderDrawLine(renderer, BZL_L, BZL_T + y * SCR_SCALE, BZL_L + SCR_W, BZL_T + y * SCR_SCALE);
    }

    SDL_SetRenderDrawColor(renderer, 0xb8, 0x70, 0x20, 0xff);
    SDL_RenderDrawLine(renderer, BZL_L + 64 * SCR_SCALE, BZL_T, BZL_L + 64 * SCR_SCALE, BZL_T + SCR_H);
    SDL_RenderDrawLine(renderer, BZL_L, BZL_T + 32 * SCR_SCALE, BZL_L + SCR_W, BZL_T + 32 * SCR_SCALE);
}

static void gui_join_thread_if_needed(pthread_t thread_id) {
    if(pthread_equal(pthread_self(), thread_id)) {
        return;
    }

    pthread_join(thread_id, NULL);
}

// TODO: multiple viewports support
static void* handle_gui(void* _view_port) {
    ViewPort* view_port = _view_port;
    while(running) {
        if(view_port->draw_callback != NULL) {
            view_port->draw_callback(view_port->gui->canvas, view_port->draw_callback_context);
            canvas_commit(view_port->gui->canvas);
        }

        const uint8_t* committed_buffer = canvas_get_committed_buffer(view_port->gui->canvas);

        SDL_SetRenderDrawColor(renderer, 0x1a, 0x1a, 0x1a, 0xff);
        SDL_RenderClear(renderer);

        render_screen_bg();

        render_debug_grid();

        SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xff);
        for(uint8_t x = 0; x < view_port->width / 8; x++)
            for(uint8_t y = 0; y < view_port->height / 8; y++)
                for(uint8_t i = 0; i < 8; i++)
                    for(uint8_t j = 0; j < 8; j++) {
                        if(!(committed_buffer[x * 8 + y * view_port->width + i] & (1 << j))) continue;
                        rect.x = BZL_L + (8 * x + i) * SCR_SCALE;
                        rect.y = BZL_T + (8 * y + j) * SCR_SCALE;
                        rect.w = SCR_SCALE;
                        rect.h = SCR_SCALE;
                        SDL_RenderFillRect(renderer, &rect);
                    }

        render_led_indicator();

        if(show_host_hud) {
            SDL_SetRenderDrawColor(renderer, 0x8a, 0x5a, 0x20, 0xff);
            rect.x = 0; rect.y = DEV_H; rect.w = WIN_W; rect.h = 2;
            SDL_RenderFillRect(renderer, &rect);

            SDL_SetRenderDrawColor(renderer, 0x8a, 0x5a, 0x20, 0xff);
            rect.x = 0; rect.y = DEV_H; rect.w = WIN_W; rect.h = 2;
            SDL_RenderFillRect(renderer, &rect);

            SDL_SetRenderDrawColor(renderer, 0xd6, 0x9a, 0x43, 0xff);
            rect.x = 0; rect.y = DEV_H + 2; rect.w = WIN_W; rect.h = HUD_H - 2;
            SDL_RenderFillRect(renderer, &rect);

            char msg_vibro[20];
            char msg_led[24];
            char msg_bl[24];
            snprintf(msg_vibro, sizeof(msg_vibro), "Vibro: %s", global_vibro_on ? "On" : "Off");
            snprintf(
                msg_led, sizeof(msg_led),
                "LED: #%02x%02x%02x",
                global_led[0], global_led[1], global_led[2]);
            snprintf(msg_bl, sizeof(msg_bl), "Backlight: %u/255", global_backlight_brightness);
            renderMessage(msg_vibro, 20, DEV_H + 12);
            renderMessage(msg_led,   20, DEV_H + 48);
            renderMessage(msg_bl,    20, DEV_H + 84);
        }

        SDL_RenderPresent(renderer);
        /* ~60 FPS */
        #ifdef _WIN32
            Sleep(16);
        #else
            usleep(16666);
        #endif
    }
    return NULL;
}
void gui_add_view_port(Gui* gui, ViewPort* view_port, GuiLayer layer) {
    UNUSED(layer);
    furi_assert(gui);
    furi_assert(view_port);
    furi_check(gui->view_port == NULL);

    running = true;
    memset(held_down, 0, sizeof(held_down));
    memset(held_time, 0, sizeof(held_time));

    gui->view_port = view_port;
    view_port->gui = gui;

#if FLIPPULATOR_HAS_SDL_TTF
    TTF_Init();
    HaxrCorp4089 = TTF_OpenFont("haxrcorp-4089.ttf", FLIPPULATOR_FONT_SIZE);
#endif

    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Init(SDL_INIT_AUDIO);
    SDL_CreateWindowAndRenderer(WIN_W, WIN_H, 0, &window, &renderer);
    SDL_SetWindowTitle(window, FLIPPULATOR_APP_NAME);

    /* Keep screen readable from app start; app code can still dim/turn off later. */
    global_backlight_brightness = 0xFF;

    SDL_zero(audio_spec);
    audio_spec.freq = AUDIO_FREQUENCY;
    audio_spec.format = AUDIO_S16SYS;
    audio_spec.channels = 1;
    audio_spec.samples = 1024;
    audio_spec.callback = sound_cb;

    audio_device = SDL_OpenAudioDevice(
        NULL, 0, &audio_spec, NULL, 0);

    SDL_PauseAudioDevice(audio_device, 0);

    pthread_create(&gui->draw_thread_id, NULL, handle_gui, view_port);
    pthread_create(&gui->input_thread_id, NULL, handle_input, view_port);
    pthread_create(&gui->input_loop_id, NULL, input_loop, view_port);
    gui->sdl_started = true;
}
void gui_remove_view_port(Gui* gui, ViewPort* view_port) {
    furi_assert(gui);
    furi_assert(view_port);

    if(gui->view_port != view_port) {
        return;
    }

    if(gui->sdl_started) {
        running = false;

        gui_join_thread_if_needed(gui->draw_thread_id);
        gui_join_thread_if_needed(gui->input_thread_id);
        gui_join_thread_if_needed(gui->input_loop_id);

        if(audio_device != 0U) {
            SDL_CloseAudioDevice(audio_device);
            audio_device = 0U;
        }

#if FLIPPULATOR_HAS_SDL_TTF
        if(HaxrCorp4089 != NULL) {
            TTF_CloseFont(HaxrCorp4089);
            HaxrCorp4089 = NULL;
        }
        TTF_Quit();
#endif

        if(renderer != NULL) {
            SDL_DestroyRenderer(renderer);
            renderer = NULL;
        }
        if(window != NULL) {
            SDL_DestroyWindow(window);
            window = NULL;
        }
        SDL_Quit();

        gui->sdl_started = false;
    }

    view_port->gui = NULL;
    gui->view_port = NULL;
}

int32_t gui_srv(void* p) {
    UNUSED(p);
    Gui* gui = gui_alloc();
    furi_record_create(RECORD_GUI, gui);
    return 0;
}
