#include "widget.h"

#include <furi.h>

#include <gui/elements.h>

typedef enum {
    WidgetElementTypeString,
    WidgetElementTypeStringMultiline,
    WidgetElementTypeButton,
    WidgetElementTypeRect,
    WidgetElementTypeCircle,
    WidgetElementTypeLine,
} WidgetElementType;

typedef struct {
    WidgetElementType type;
    union {
        struct {
            uint8_t x;
            uint8_t y;
            Align horizontal;
            Align vertical;
            Font font;
            char* text;
        } string;
        struct {
            GuiButtonType button_type;
            char* text;
            ButtonCallback callback;
            void* context;
        } button;
        struct {
            uint8_t x;
            uint8_t y;
            uint8_t w;
            uint8_t h;
            uint8_t radius;
            bool fill;
        } rect;
        struct {
            uint8_t x;
            uint8_t y;
            uint8_t radius;
            bool fill;
        } circle;
        struct {
            uint8_t x1;
            uint8_t y1;
            uint8_t x2;
            uint8_t y2;
        } line;
    };
} WidgetElement;

struct Widget {
    View* view;
    WidgetElement* elements;
    size_t element_count;
    size_t element_capacity;
};

static void widget_add_element(Widget* widget, const WidgetElement* element) {
    if(widget->element_count == widget->element_capacity) {
        const size_t new_capacity = widget->element_capacity == 0U ? 8U : widget->element_capacity * 2U;
        widget->elements = realloc(widget->elements, new_capacity * sizeof(WidgetElement));
        widget->element_capacity = new_capacity;
    }

    widget->elements[widget->element_count] = *element;
    widget->element_count++;
}

static void widget_view_draw(Canvas* canvas, void* model) {
    Widget* widget = model;
    canvas_clear(canvas);

    for(size_t i = 0; i < widget->element_count; i++) {
        WidgetElement* element = &widget->elements[i];
        switch(element->type) {
        case WidgetElementTypeString:
            canvas_set_font(canvas, element->string.font);
            canvas_draw_str_aligned(
                canvas,
                element->string.x,
                element->string.y,
                element->string.horizontal,
                element->string.vertical,
                element->string.text);
            break;
        case WidgetElementTypeStringMultiline:
            canvas_set_font(canvas, element->string.font);
            elements_multiline_text_aligned(
                canvas,
                element->string.x,
                element->string.y,
                element->string.horizontal,
                element->string.vertical,
                element->string.text);
            break;
        case WidgetElementTypeButton:
            if(element->button.button_type == GuiButtonTypeLeft) {
                elements_button_left(canvas, element->button.text);
            } else if(element->button.button_type == GuiButtonTypeCenter) {
                elements_button_center(canvas, element->button.text);
            } else if(element->button.button_type == GuiButtonTypeRight) {
                elements_button_right(canvas, element->button.text);
            }
            break;
        case WidgetElementTypeRect:
            if(element->rect.fill) {
                if(element->rect.radius == 0U) {
                    canvas_draw_box(
                        canvas, element->rect.x, element->rect.y, element->rect.w, element->rect.h);
                } else {
                    canvas_draw_rbox(
                        canvas,
                        element->rect.x,
                        element->rect.y,
                        element->rect.w,
                        element->rect.h,
                        element->rect.radius);
                }
            } else if(element->rect.radius == 0U) {
                canvas_draw_frame(
                    canvas, element->rect.x, element->rect.y, element->rect.w, element->rect.h);
            } else {
                canvas_draw_rframe(
                    canvas,
                    element->rect.x,
                    element->rect.y,
                    element->rect.w,
                    element->rect.h,
                    element->rect.radius);
            }
            break;
        case WidgetElementTypeCircle:
            if(element->circle.fill) {
                canvas_draw_disc(
                    canvas, element->circle.x, element->circle.y, element->circle.radius);
            } else {
                canvas_draw_circle(
                    canvas, element->circle.x, element->circle.y, element->circle.radius);
            }
            break;
        case WidgetElementTypeLine:
            canvas_draw_line(
                canvas, element->line.x1, element->line.y1, element->line.x2, element->line.y2);
            break;
        }
    }
}

static bool widget_view_input(InputEvent* event, void* context) {
    Widget* widget = context;
    if(event == NULL) {
        return false;
    }

    GuiButtonType button_type;
    bool matched = true;
    if(event->key == InputKeyLeft) {
        button_type = GuiButtonTypeLeft;
    } else if(event->key == InputKeyOk) {
        button_type = GuiButtonTypeCenter;
    } else if(event->key == InputKeyRight) {
        button_type = GuiButtonTypeRight;
    } else {
        matched = false;
    }

    if(!matched) {
        return false;
    }

    for(size_t i = 0; i < widget->element_count; i++) {
        WidgetElement* element = &widget->elements[i];
        if(element->type == WidgetElementTypeButton &&
           element->button.button_type == button_type &&
           element->button.callback != NULL) {
            element->button.callback(button_type, event->type, element->button.context);
            return true;
        }
    }

    return false;
}

Widget* widget_alloc(void) {
    Widget* widget = calloc(1, sizeof(Widget));
    widget->view = view_alloc();
    view_set_context(widget->view, widget);
    view_set_input_callback(widget->view, widget_view_input);
    view_set_draw_callback(widget->view, widget_view_draw);
    view_allocate_model(widget->view, ViewModelTypeLockFree, sizeof(Widget));
    Widget* model = view_get_model(widget->view);
    *model = *widget;
    view_commit_model(widget->view, false);
    return widget;
}

static void widget_sync_view_model(Widget* widget, bool update) {
    Widget* model = view_get_model(widget->view);
    *model = *widget;
    view_commit_model(widget->view, update);
}

static void widget_free_element(WidgetElement* element) {
    if(element->type == WidgetElementTypeString ||
       element->type == WidgetElementTypeStringMultiline) {
        free(element->string.text);
    } else if(element->type == WidgetElementTypeButton) {
        free(element->button.text);
    }
}

void widget_free(Widget* widget) {
    furi_assert(widget);
    widget_reset(widget);
    view_free(widget->view);
    free(widget);
}

void widget_reset(Widget* widget) {
    furi_assert(widget);
    for(size_t i = 0; i < widget->element_count; i++) {
        widget_free_element(&widget->elements[i]);
    }
    free(widget->elements);
    widget->elements = NULL;
    widget->element_count = 0U;
    widget->element_capacity = 0U;
    widget_sync_view_model(widget, true);
}

View* widget_get_view(Widget* widget) {
    return widget->view;
}

void widget_add_string_multiline_element(
    Widget* widget,
    uint8_t x,
    uint8_t y,
    Align horizontal,
    Align vertical,
    Font font,
    const char* text) {
    WidgetElement element = {
        .type = WidgetElementTypeStringMultiline,
        .string =
            {
                .x = x,
                .y = y,
                .horizontal = horizontal,
                .vertical = vertical,
                .font = font,
                .text = strdup(text),
            },
    };
    widget_add_element(widget, &element);
    widget_sync_view_model(widget, true);
}

void widget_add_string_element(
    Widget* widget,
    uint8_t x,
    uint8_t y,
    Align horizontal,
    Align vertical,
    Font font,
    const char* text) {
    WidgetElement element = {
        .type = WidgetElementTypeString,
        .string =
            {
                .x = x,
                .y = y,
                .horizontal = horizontal,
                .vertical = vertical,
                .font = font,
                .text = strdup(text),
            },
    };
    widget_add_element(widget, &element);
    widget_sync_view_model(widget, true);
}

void widget_add_button_element(
    Widget* widget,
    GuiButtonType button_type,
    const char* text,
    ButtonCallback callback,
    void* context) {
    WidgetElement element = {
        .type = WidgetElementTypeButton,
        .button =
            {
                .button_type = button_type,
                .text = strdup(text),
                .callback = callback,
                .context = context,
            },
    };
    widget_add_element(widget, &element);
    widget_sync_view_model(widget, true);
}

void widget_add_rect_element(
    Widget* widget,
    uint8_t x,
    uint8_t y,
    uint8_t width,
    uint8_t height,
    uint8_t radius,
    bool fill) {
    WidgetElement element = {
        .type = WidgetElementTypeRect,
        .rect = {.x = x, .y = y, .w = width, .h = height, .radius = radius, .fill = fill},
    };
    widget_add_element(widget, &element);
    widget_sync_view_model(widget, true);
}

void widget_add_circle_element(
    Widget* widget,
    uint8_t x,
    uint8_t y,
    uint8_t radius,
    bool fill) {
    WidgetElement element = {
        .type = WidgetElementTypeCircle,
        .circle = {.x = x, .y = y, .radius = radius, .fill = fill},
    };
    widget_add_element(widget, &element);
    widget_sync_view_model(widget, true);
}

void widget_add_line_element(
    Widget* widget,
    uint8_t x1,
    uint8_t y1,
    uint8_t x2,
    uint8_t y2) {
    WidgetElement element = {
        .type = WidgetElementTypeLine,
        .line = {.x1 = x1, .y1 = y1, .x2 = x2, .y2 = y2},
    };
    widget_add_element(widget, &element);
    widget_sync_view_model(widget, true);
}
