#include "elements.h"

#include <furi.h>
#include <string.h>

enum {
    ElementsButtonHeight = 11U,
    ElementsButtonMargin = 2U,
    ElementsButtonPadding = 3U,
    ElementsButtonIconGap = 4U,
    ElementsButtonIconBase = 5U,
    ElementsButtonIconHeight = 5U,
    ElementsProgressBarHeight = 9U,
    ElementsScrollbarWidth = 3U,
    ElementsScrollbarKnobMinHeight = 8U,
};

static uint8_t elements_clamp_u8_i32(int32_t value) {
    if(value <= 0) {
        return 0U;
    }
    if(value >= UINT8_MAX) {
        return UINT8_MAX;
    }
    return (uint8_t)value;
}

static uint8_t elements_clamp_u8_size(size_t value) {
    if(value >= UINT8_MAX) {
        return UINT8_MAX;
    }
    return (uint8_t)value;
}

static uint8_t elements_button_baseline(uint8_t y) {
    return y + ElementsButtonHeight - 3U;
}

static void elements_draw_button(
    Canvas* canvas,
    uint8_t x,
    uint8_t y,
    const char* str,
    Align align,
    CanvasDirection icon_dir) {
    furi_assert(canvas);
    if(str == NULL) {
        return;
    }

    canvas_set_font(canvas, FontSecondary);

    const uint8_t text_width = elements_clamp_u8_size(canvas_string_width(canvas, str));
    const uint8_t width =
        text_width + (2U * ElementsButtonPadding) + ElementsButtonIconBase + ElementsButtonIconGap;
    uint8_t left = x;

    if(align == AlignCenter) {
        const uint8_t canvas_w = canvas_width(canvas);
        left = (canvas_w > width) ? (canvas_w - width) / 2U : 0U;
    } else if(align == AlignRight) {
        const uint8_t canvas_w = canvas_width(canvas);
        const uint16_t required_width = (uint16_t)width + ElementsButtonMargin;
        left = (canvas_w > required_width) ? (uint8_t)(canvas_w - required_width) : 0U;
    }

    canvas_draw_rbox(canvas, left, y, width, ElementsButtonHeight, 2U);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_triangle(
        canvas,
        left + ElementsButtonPadding + (ElementsButtonIconBase / 2U),
        y + (ElementsButtonHeight / 2U),
        ElementsButtonIconBase,
        ElementsButtonIconHeight,
        icon_dir);
    canvas_draw_str(
        canvas,
        left + ElementsButtonPadding + ElementsButtonIconBase + ElementsButtonIconGap,
        elements_button_baseline(y),
        str);
    canvas_set_color(canvas, ColorBlack);
}

void elements_progress_bar(Canvas* canvas, int32_t x, int32_t y, size_t width, float progress) {
    furi_assert(canvas);
    if(width == 0U) {
        return;
    }

    if(progress < 0.0f) {
        progress = 0.0f;
    } else if(progress > 1.0f) {
        progress = 1.0f;
    }

    const uint8_t px = elements_clamp_u8_i32(x);
    const uint8_t py = elements_clamp_u8_i32(y);
    const uint8_t pwidth = elements_clamp_u8_size(width);

    canvas_draw_rframe(canvas, px, py, pwidth, ElementsProgressBarHeight, 2U);

    const uint8_t inner_width = pwidth > 2U ? pwidth - 2U : 0U;
    if(inner_width == 0U) {
        return;
    }

    const uint8_t fill_width = (uint8_t)(progress * inner_width);
    if(fill_width > 0U) {
        canvas_draw_rbox(canvas, px + 1U, py + 1U, fill_width, ElementsProgressBarHeight - 2U, 1U);
    }
}

void elements_progress_bar_with_text(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    size_t width,
    float progress,
    const char* text) {
    furi_assert(canvas);
    elements_progress_bar(canvas, x, y, width, progress);

    if(text == NULL) {
        return;
    }

    canvas_set_font(canvas, FontSecondary);
    canvas_set_color(canvas, ColorXOR);
    canvas_draw_str_aligned(
        canvas,
        elements_clamp_u8_i32(x) + (elements_clamp_u8_size(width) / 2U),
        elements_clamp_u8_i32(y) + ElementsProgressBarHeight - 2U,
        AlignCenter,
        AlignBottom,
        text);
    canvas_set_color(canvas, ColorBlack);
}

void elements_scrollbar_pos(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    size_t height,
    size_t pos,
    size_t total) {
    furi_assert(canvas);
    if(height == 0U || total == 0U) {
        return;
    }

    const uint8_t px = elements_clamp_u8_i32(x);
    const uint8_t py = elements_clamp_u8_i32(y);
    const uint8_t pheight = elements_clamp_u8_size(height);

    canvas_draw_frame(canvas, px, py, ElementsScrollbarWidth, pheight);

    if(total <= 1U || pheight <= 2U) {
        return;
    }

    const uint8_t track_height = pheight - 2U;
    size_t knob_height = (track_height * track_height) / total;
    if(knob_height < ElementsScrollbarKnobMinHeight) {
        knob_height = ElementsScrollbarKnobMinHeight;
    }
    if(knob_height > track_height) {
        knob_height = track_height;
    }

    const size_t max_offset = track_height - knob_height;
    const size_t clamped_pos = pos >= total ? total - 1U : pos;
    const uint8_t knob_offset =
        (uint8_t)((max_offset * clamped_pos) / (total > 1U ? total - 1U : 1U));
    canvas_draw_box(canvas, px + 1U, py + 1U + knob_offset, 1U, (uint8_t)knob_height);
}

void elements_scrollbar(Canvas* canvas, size_t pos, size_t total) {
    furi_assert(canvas);
    const uint8_t width = canvas_width(canvas);
    const uint8_t height = canvas_height(canvas);

    if(width <= ElementsScrollbarWidth || height <= 2U) {
        return;
    }

    elements_scrollbar_pos(canvas, width - ElementsScrollbarWidth, 0, height, pos, total);
}

void elements_frame(Canvas* canvas, int32_t x, int32_t y, size_t width, size_t height) {
    furi_assert(canvas);
    if(width == 0U || height == 0U) {
        return;
    }

    canvas_draw_rframe(
        canvas,
        elements_clamp_u8_i32(x),
        elements_clamp_u8_i32(y),
        elements_clamp_u8_size(width),
        elements_clamp_u8_size(height),
        2U);
}

void elements_button_left(Canvas* canvas, const char* str) {
    furi_assert(canvas);
    elements_draw_button(
        canvas, ElementsButtonMargin, canvas_height(canvas) - ElementsButtonHeight, str, AlignLeft, CanvasDirectionRightToLeft);
}

void elements_button_right(Canvas* canvas, const char* str) {
    furi_assert(canvas);
    elements_draw_button(
        canvas, 0U, canvas_height(canvas) - ElementsButtonHeight, str, AlignRight, CanvasDirectionLeftToRight);
}

void elements_button_up(Canvas* canvas, const char* str) {
    furi_assert(canvas);
    elements_draw_button(canvas, ElementsButtonMargin, ElementsButtonMargin, str, AlignLeft, CanvasDirectionBottomToTop);
}

void elements_button_down(Canvas* canvas, const char* str) {
    furi_assert(canvas);
    elements_draw_button(canvas, 0U, ElementsButtonMargin, str, AlignRight, CanvasDirectionTopToBottom);
}

void elements_button_center(Canvas* canvas, const char* str) {
    furi_assert(canvas);
    elements_draw_button(
        canvas, 0U, canvas_height(canvas) - ElementsButtonHeight, str, AlignCenter, CanvasDirectionTopToBottom);
}

void elements_multiline_text_aligned(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    Align horizontal,
    Align vertical,
    const char* text) {
    furi_assert(canvas);
    if(text == NULL || text[0] == '\0') {
        return;
    }

    const uint8_t anchor_x = elements_clamp_u8_i32(x);
    uint8_t anchor_y = elements_clamp_u8_i32(y);
    const uint8_t line_height = canvas_current_font_height(canvas) + 1U;

    size_t line_count = 1U;
    size_t max_width = 0U;
    const char* line = text;
    const char* cursor = text;

    while(true) {
        if(*cursor == '\n' || *cursor == '\0') {
            const size_t line_size = (size_t)(cursor - line);
            char line_buffer[128];
            const size_t copy_size = MIN(line_size, sizeof(line_buffer) - 1U);
            memcpy(line_buffer, line, copy_size);
            line_buffer[copy_size] = '\0';

            const size_t current_width = canvas_string_width(canvas, line_buffer);
            if(current_width > max_width) {
                max_width = current_width;
            }

            if(*cursor == '\0') {
                break;
            }

            line = cursor + 1;
            line_count++;
        }
        cursor++;
    }

    if(vertical == AlignCenter) {
        const size_t block_height = line_count * line_height;
        if(anchor_y >= block_height / 2U) {
            anchor_y = (uint8_t)(anchor_y - (block_height / 2U));
        } else {
            anchor_y = 0U;
        }
    } else if(vertical == AlignBottom) {
        const size_t block_height = line_count * line_height;
        if(anchor_y >= block_height) {
            anchor_y = (uint8_t)(anchor_y - block_height);
        } else {
            anchor_y = 0U;
        }
    }

    line = text;
    cursor = text;
    size_t line_index = 0U;
    while(true) {
        if(*cursor == '\n' || *cursor == '\0') {
            const size_t line_size = (size_t)(cursor - line);
            char line_buffer[128];
            const size_t copy_size = MIN(line_size, sizeof(line_buffer) - 1U);
            memcpy(line_buffer, line, copy_size);
            line_buffer[copy_size] = '\0';

            uint8_t line_x = anchor_x;
            if(horizontal == AlignCenter) {
                const size_t current_width = canvas_string_width(canvas, line_buffer);
                const size_t centered_x =
                    (anchor_x > current_width / 2U) ? anchor_x - (current_width / 2U) : 0U;
                line_x = elements_clamp_u8_size(centered_x);
            } else if(horizontal == AlignRight) {
                const size_t current_width = canvas_string_width(canvas, line_buffer);
                const size_t right_x = (anchor_x > current_width) ? anchor_x - current_width : 0U;
                line_x = elements_clamp_u8_size(right_x);
            }

            canvas_draw_str(canvas, line_x, anchor_y + ((line_index + 1U) * line_height), line_buffer);

            if(*cursor == '\0') {
                break;
            }

            line = cursor + 1;
            line_index++;
        }
        cursor++;
    }
}
