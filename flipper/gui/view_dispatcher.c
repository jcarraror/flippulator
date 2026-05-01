#include "view_dispatcher.h"

#include <furi.h>

typedef struct {
    uint32_t id;
    View* view;
} ViewDispatcherEntry;

struct ViewDispatcher {
    FuriMessageQueue* input_queue;
    FuriMessageQueue* event_queue;
    Gui* gui;
    ViewPort* view_port;
    ViewDispatcherEntry* entries;
    size_t entry_count;
    size_t entry_capacity;
    View* current_view;
    uint32_t current_view_id;
    ViewDispatcherCustomEventCallback custom_event_callback;
    ViewDispatcherNavigationEventCallback navigation_event_callback;
    ViewDispatcherTickEventCallback tick_event_callback;
    uint32_t tick_period;
    void* event_context;
    bool running;
    ViewDispatcherType type;
};

static uint64_t view_dispatcher_now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ((uint64_t)ts.tv_sec * 1000U) + (uint64_t)(ts.tv_nsec / 1000000U);
}

static ViewDispatcherEntry* view_dispatcher_find_entry(ViewDispatcher* view_dispatcher, uint32_t id) {
    for(size_t i = 0; i < view_dispatcher->entry_count; i++) {
        if(view_dispatcher->entries[i].id == id) {
            return &view_dispatcher->entries[i];
        }
    }
    return NULL;
}

static void view_dispatcher_draw_callback(Canvas* canvas, void* context) {
    ViewDispatcher* view_dispatcher = context;
    if(view_dispatcher->current_view != NULL) {
        view_draw(view_dispatcher->current_view, canvas);
    } else {
        canvas_clear(canvas);
    }
}

static void view_dispatcher_input_callback(InputEvent* event, void* context) {
    ViewDispatcher* view_dispatcher = context;
    if(event == NULL) {
        return;
    }
    furi_message_queue_put(view_dispatcher->input_queue, event, FuriWaitForever);
}

static void view_dispatcher_update(View* view, void* context) {
    UNUSED(view);
    ViewDispatcher* view_dispatcher = context;
    if(view_dispatcher->view_port != NULL) {
        view_port_update(view_dispatcher->view_port);
    }
}

static void view_dispatcher_set_current_view(ViewDispatcher* view_dispatcher, View* view) {
    if(view_dispatcher->current_view == view) {
        return;
    }

    if(view_dispatcher->current_view != NULL) {
        view_exit(view_dispatcher->current_view);
    }

    view_dispatcher->current_view = view;

    if(view_dispatcher->current_view != NULL) {
        view_enter(view_dispatcher->current_view);
    }

    if(view_dispatcher->view_port != NULL) {
        view_port_update(view_dispatcher->view_port);
    }
}

static bool view_dispatcher_handle_navigation(ViewDispatcher* view_dispatcher) {
    if(view_dispatcher->current_view == NULL) {
        return false;
    }

    const uint32_t previous_view = view_previous(view_dispatcher->current_view);
    if(previous_view == VIEW_IGNORE) {
        return true;
    }

    if(previous_view != VIEW_NONE) {
        view_dispatcher_switch_to_view(view_dispatcher, previous_view);
        return true;
    }

    if(view_dispatcher->navigation_event_callback != NULL) {
        if(view_dispatcher->navigation_event_callback(view_dispatcher->event_context)) {
            return true;
        }
    }

    view_dispatcher_stop(view_dispatcher);
    return true;
}

static void view_dispatcher_handle_input(ViewDispatcher* view_dispatcher, InputEvent* event) {
    if(view_dispatcher->current_view == NULL || event == NULL) {
        return;
    }

    const bool handled = view_input(view_dispatcher->current_view, event);
    if(!handled && event->key == InputKeyBack && event->type == InputTypeShort) {
        view_dispatcher_handle_navigation(view_dispatcher);
    }
}

static void view_dispatcher_handle_custom_event(ViewDispatcher* view_dispatcher, uint32_t event) {
    bool handled = false;
    if(view_dispatcher->current_view != NULL) {
        handled = view_custom(view_dispatcher->current_view, event);
    }

    if(!handled && view_dispatcher->custom_event_callback != NULL) {
        view_dispatcher->custom_event_callback(view_dispatcher->event_context, event);
    }
}

ViewDispatcher* view_dispatcher_alloc(void) {
    ViewDispatcher* view_dispatcher = calloc(1, sizeof(ViewDispatcher));
    view_dispatcher->input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    view_dispatcher->event_queue = furi_message_queue_alloc(8, sizeof(uint32_t));
    view_dispatcher->current_view_id = VIEW_NONE;
    return view_dispatcher;
}

void view_dispatcher_free(ViewDispatcher* view_dispatcher) {
    furi_assert(view_dispatcher);
    if(view_dispatcher->view_port != NULL) {
        if(view_dispatcher->gui != NULL) {
            gui_remove_view_port(view_dispatcher->gui, view_dispatcher->view_port);
            view_dispatcher->gui = NULL;
        }
        view_port_free(view_dispatcher->view_port);
        view_dispatcher->view_port = NULL;
    }
    furi_message_queue_free(view_dispatcher->input_queue);
    furi_message_queue_free(view_dispatcher->event_queue);
    free(view_dispatcher->entries);
    free(view_dispatcher);
}

void view_dispatcher_enable_queue(ViewDispatcher* view_dispatcher) {
    UNUSED(view_dispatcher);
}

void view_dispatcher_send_custom_event(ViewDispatcher* view_dispatcher, uint32_t event) {
    furi_message_queue_put(view_dispatcher->event_queue, &event, FuriWaitForever);
}

void view_dispatcher_set_custom_event_callback(
    ViewDispatcher* view_dispatcher,
    ViewDispatcherCustomEventCallback callback) {
    view_dispatcher->custom_event_callback = callback;
}

void view_dispatcher_set_navigation_event_callback(
    ViewDispatcher* view_dispatcher,
    ViewDispatcherNavigationEventCallback callback) {
    view_dispatcher->navigation_event_callback = callback;
}

void view_dispatcher_set_tick_event_callback(
    ViewDispatcher* view_dispatcher,
    ViewDispatcherTickEventCallback callback,
    uint32_t tick_period) {
    view_dispatcher->tick_event_callback = callback;
    view_dispatcher->tick_period = tick_period;
}

void view_dispatcher_set_event_callback_context(ViewDispatcher* view_dispatcher, void* context) {
    view_dispatcher->event_context = context;
}

void* view_dispatcher_get_event_loop(ViewDispatcher* view_dispatcher) {
    UNUSED(view_dispatcher);
    return NULL;
}

void view_dispatcher_run(ViewDispatcher* view_dispatcher) {
    furi_assert(view_dispatcher);
    view_dispatcher->running = true;

    uint64_t next_tick = view_dispatcher_now_ms() + view_dispatcher->tick_period;

    while(view_dispatcher->running) {
        bool did_work = false;

        while(furi_message_queue_get_count(view_dispatcher->event_queue) > 0U) {
            uint32_t event = 0U;
            furi_check(
                furi_message_queue_get(view_dispatcher->event_queue, &event, FuriWaitForever) ==
                FuriStatusOk);
            view_dispatcher_handle_custom_event(view_dispatcher, event);
            did_work = true;
        }

        while(furi_message_queue_get_count(view_dispatcher->input_queue) > 0U) {
            InputEvent event;
            furi_check(
                furi_message_queue_get(view_dispatcher->input_queue, &event, FuriWaitForever) ==
                FuriStatusOk);
            view_dispatcher_handle_input(view_dispatcher, &event);
            did_work = true;
        }

        if(view_dispatcher->tick_event_callback != NULL && view_dispatcher->tick_period > 0U) {
            const uint64_t now = view_dispatcher_now_ms();
            if(now >= next_tick) {
                view_dispatcher->tick_event_callback(view_dispatcher->event_context);
                next_tick = now + view_dispatcher->tick_period;
                did_work = true;
            }
        }

        if(!did_work) {
            furi_delay_ms(10);
        }
    }
}

void view_dispatcher_stop(ViewDispatcher* view_dispatcher) {
    view_dispatcher->running = false;
}

void view_dispatcher_add_view(ViewDispatcher* view_dispatcher, uint32_t view_id, View* view) {
    furi_assert(view_dispatcher);
    furi_assert(view);

    ViewDispatcherEntry* entry = view_dispatcher_find_entry(view_dispatcher, view_id);
    if(entry != NULL) {
        entry->view = view;
    } else {
        if(view_dispatcher->entry_count == view_dispatcher->entry_capacity) {
            const size_t new_capacity = view_dispatcher->entry_capacity == 0U ?
                                            4U :
                                            view_dispatcher->entry_capacity * 2U;
            view_dispatcher->entries =
                realloc(view_dispatcher->entries, new_capacity * sizeof(ViewDispatcherEntry));
            view_dispatcher->entry_capacity = new_capacity;
        }

        view_dispatcher->entries[view_dispatcher->entry_count].id = view_id;
        view_dispatcher->entries[view_dispatcher->entry_count].view = view;
        view_dispatcher->entry_count++;
    }

    view_set_update_callback(view, view_dispatcher_update);
    view_set_update_callback_context(view, view_dispatcher);
}

void view_dispatcher_remove_view(ViewDispatcher* view_dispatcher, uint32_t view_id) {
    furi_assert(view_dispatcher);
    for(size_t i = 0; i < view_dispatcher->entry_count; i++) {
        if(view_dispatcher->entries[i].id == view_id) {
            if(view_dispatcher->current_view == view_dispatcher->entries[i].view) {
                view_dispatcher_set_current_view(view_dispatcher, NULL);
                view_dispatcher->current_view_id = VIEW_NONE;
            }

            if(i + 1U < view_dispatcher->entry_count) {
                memmove(
                    &view_dispatcher->entries[i],
                    &view_dispatcher->entries[i + 1U],
                    (view_dispatcher->entry_count - i - 1U) * sizeof(ViewDispatcherEntry));
            }
            view_dispatcher->entry_count--;
            break;
        }
    }
}

void view_dispatcher_switch_to_view(ViewDispatcher* view_dispatcher, uint32_t view_id) {
    ViewDispatcherEntry* entry = view_dispatcher_find_entry(view_dispatcher, view_id);
    furi_assert(entry != NULL);
    view_dispatcher->current_view_id = view_id;
    view_dispatcher_set_current_view(view_dispatcher, entry->view);
}

void view_dispatcher_send_to_front(ViewDispatcher* view_dispatcher) {
    UNUSED(view_dispatcher);
}

void view_dispatcher_send_to_back(ViewDispatcher* view_dispatcher) {
    UNUSED(view_dispatcher);
}

void view_dispatcher_attach_to_gui(
    ViewDispatcher* view_dispatcher,
    Gui* gui,
    ViewDispatcherType type) {
    furi_assert(view_dispatcher);
    furi_assert(gui);

    view_dispatcher->gui = gui;
    view_dispatcher->type = type;
    if(view_dispatcher->view_port == NULL) {
        view_dispatcher->view_port = view_port_alloc();
        view_port_draw_callback_set(
            view_dispatcher->view_port, view_dispatcher_draw_callback, view_dispatcher);
        view_port_input_callback_set(
            view_dispatcher->view_port, view_dispatcher_input_callback, view_dispatcher);
    }

    GuiLayer layer = GuiLayerFullscreen;
    if(type == ViewDispatcherTypeWindow) {
        layer = GuiLayerWindow;
    } else if(type == ViewDispatcherTypeDesktop) {
        layer = GuiLayerDesktop;
    }

    gui_add_view_port(gui, view_dispatcher->view_port, layer);
}
