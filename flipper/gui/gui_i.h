#pragma once

#include <pthread.h>

#include "gui.h"
#include "canvas_i.h"
#include "view_port_i.h"

#define GUI_DISPLAY_WIDTH 128
#define GUI_DISPLAY_HEIGHT 64

struct Gui {
    Canvas* canvas;
    ViewPort* view_port;
    pthread_t draw_thread_id;
    pthread_t input_thread_id;
    pthread_t input_loop_id;
    bool sdl_started;
};