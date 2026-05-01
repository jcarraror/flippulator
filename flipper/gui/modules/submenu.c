#include "submenu.h"

#include <furi.h>

#include <gui/elements.h>

typedef struct {
    char* label;
    uint32_t index;
    SubmenuItemCallback callback;
    SubmenuItemCallbackEx callback_ex;
    void* context;
} SubmenuItem;

struct Submenu {
    View* view;
    char* header;
    SubmenuItem* items;
    size_t item_count;
    size_t item_capacity;
    size_t selected_position;
};

static void submenu_sync_view_model(Submenu* submenu, bool update) {
    Submenu* model = view_get_model(submenu->view);
    *model = *submenu;
    view_commit_model(submenu->view, update);
}

static void submenu_draw_callback(Canvas* canvas, void* model) {
    Submenu* submenu = model;
    canvas_clear(canvas);

    if(submenu->header != NULL) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignBottom, submenu->header);
        canvas_draw_line(canvas, 0, 13, 127, 13);
    }

    canvas_set_font(canvas, FontSecondary);
    for(size_t i = 0; i < submenu->item_count; i++) {
        const uint8_t y = (uint8_t)(20U + i * 10U);
        if(i == submenu->selected_position) {
            canvas_draw_rbox(canvas, 6, (uint8_t)(y - 7U), 116, 9, 2U);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str(canvas, 10, y, submenu->items[i].label);
            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_draw_str(canvas, 10, y, submenu->items[i].label);
        }
    }
}

static bool submenu_input_callback(InputEvent* event, void* context) {
    Submenu* submenu = context;
    if(event == NULL) {
        return false;
    }

    if(event->type != InputTypePress && event->type != InputTypeRepeat && event->type != InputTypeShort) {
        return false;
    }

    if(event->key == InputKeyUp) {
        if(submenu->item_count > 0U) {
            if(submenu->selected_position == 0U) {
                submenu->selected_position = submenu->item_count - 1U;
            } else {
                submenu->selected_position--;
            }
            submenu_sync_view_model(submenu, true);
        }
        return true;
    }

    if(event->key == InputKeyDown) {
        if(submenu->item_count > 0U) {
            submenu->selected_position = (submenu->selected_position + 1U) % submenu->item_count;
            submenu_sync_view_model(submenu, true);
        }
        return true;
    }

    if(event->key == InputKeyOk && submenu->item_count > 0U) {
        SubmenuItem* item = &submenu->items[submenu->selected_position];
        if(item->callback_ex != NULL) {
            item->callback_ex(item->context, event->type, item->index);
        } else if(item->callback != NULL && event->type == InputTypePress) {
            item->callback(item->context, item->index);
        }
        return true;
    }

    return false;
}

Submenu* submenu_alloc(void) {
    Submenu* submenu = calloc(1, sizeof(Submenu));
    submenu->view = view_alloc();
    view_set_context(submenu->view, submenu);
    view_set_draw_callback(submenu->view, submenu_draw_callback);
    view_set_input_callback(submenu->view, submenu_input_callback);
    view_allocate_model(submenu->view, ViewModelTypeLockFree, sizeof(Submenu));
    submenu_sync_view_model(submenu, false);
    return submenu;
}

void submenu_free(Submenu* submenu) {
    furi_assert(submenu);
    submenu_reset(submenu);
    free(submenu->header);
    view_free(submenu->view);
    free(submenu);
}

View* submenu_get_view(Submenu* submenu) {
    return submenu->view;
}

static void submenu_add_item_internal(
    Submenu* submenu,
    const char* label,
    uint32_t index,
    SubmenuItemCallback callback,
    SubmenuItemCallbackEx callback_ex,
    void* callback_context) {
    if(submenu->item_count == submenu->item_capacity) {
        const size_t new_capacity = submenu->item_capacity == 0U ? 4U : submenu->item_capacity * 2U;
        submenu->items = realloc(submenu->items, new_capacity * sizeof(SubmenuItem));
        submenu->item_capacity = new_capacity;
    }

    submenu->items[submenu->item_count].label = strdup(label);
    submenu->items[submenu->item_count].index = index;
    submenu->items[submenu->item_count].callback = callback;
    submenu->items[submenu->item_count].callback_ex = callback_ex;
    submenu->items[submenu->item_count].context = callback_context;
    submenu->item_count++;
    submenu_sync_view_model(submenu, true);
}

void submenu_add_item(
    Submenu* submenu,
    const char* label,
    uint32_t index,
    SubmenuItemCallback callback,
    void* callback_context) {
    submenu_add_item_internal(submenu, label, index, callback, NULL, callback_context);
}

void submenu_add_item_ex(
    Submenu* submenu,
    const char* label,
    uint32_t index,
    SubmenuItemCallbackEx callback,
    void* callback_context) {
    submenu_add_item_internal(submenu, label, index, NULL, callback, callback_context);
}

void submenu_change_item_label(Submenu* submenu, uint32_t index, const char* label) {
    for(size_t i = 0; i < submenu->item_count; i++) {
        if(submenu->items[i].index == index) {
            free(submenu->items[i].label);
            submenu->items[i].label = strdup(label);
            submenu_sync_view_model(submenu, true);
            return;
        }
    }
}

void submenu_reset(Submenu* submenu) {
    for(size_t i = 0; i < submenu->item_count; i++) {
        free(submenu->items[i].label);
    }
    free(submenu->items);
    submenu->items = NULL;
    submenu->item_count = 0U;
    submenu->item_capacity = 0U;
    submenu->selected_position = 0U;
    submenu_sync_view_model(submenu, true);
}

uint32_t submenu_get_selected_item(Submenu* submenu) {
    if(submenu->item_count == 0U) {
        return 0U;
    }
    return submenu->items[submenu->selected_position].index;
}

void submenu_set_selected_item(Submenu* submenu, uint32_t index) {
    for(size_t i = 0; i < submenu->item_count; i++) {
        if(submenu->items[i].index == index) {
            submenu->selected_position = i;
            submenu_sync_view_model(submenu, true);
            return;
        }
    }
}

void submenu_set_header(Submenu* submenu, const char* header) {
    free(submenu->header);
    submenu->header = header != NULL ? strdup(header) : NULL;
    submenu_sync_view_model(submenu, true);
}
