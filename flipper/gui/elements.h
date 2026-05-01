#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "canvas.h"

#ifdef __cplusplus
extern "C" {
#endif

void elements_progress_bar(Canvas* canvas, int32_t x, int32_t y, size_t width, float progress);
void elements_progress_bar_with_text(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    size_t width,
    float progress,
    const char* text);
void elements_scrollbar_pos(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    size_t height,
    size_t pos,
    size_t total);
void elements_scrollbar(Canvas* canvas, size_t pos, size_t total);
void elements_frame(Canvas* canvas, int32_t x, int32_t y, size_t width, size_t height);
void elements_button_left(Canvas* canvas, const char* str);
void elements_button_right(Canvas* canvas, const char* str);
void elements_button_up(Canvas* canvas, const char* str);
void elements_button_down(Canvas* canvas, const char* str);
void elements_button_center(Canvas* canvas, const char* str);
void elements_multiline_text_aligned(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    Align horizontal,
    Align vertical,
    const char* text);

#ifdef __cplusplus
}
#endif
