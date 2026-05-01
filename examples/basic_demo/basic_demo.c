#include <furi.h>

#include <gui/gui.h>
#include <gui/view_port.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#include <stdio.h>
#include <stdlib.h>

typedef struct {
    FuriMessageQueue* input_queue;
    ViewPort* view_port;
    Gui* gui;
    NotificationApp* notification;
    uint8_t cursor_x;
    uint8_t cursor_y;
    bool filled;
    bool running;
} BasicDemoApp;

static void basic_demo_wait_for_services(BasicDemoApp* app) {
    while(app->gui == NULL) {
        app->gui = furi_record_open(RECORD_GUI);
        if(app->gui == NULL) {
            furi_delay_ms(10);
        }
    }

    while(app->notification == NULL) {
        app->notification = furi_record_open(RECORD_NOTIFICATION);
        if(app->notification == NULL) {
            furi_delay_ms(10);
        }
    }
}

static uint8_t basic_demo_clamp(int32_t value, uint8_t min, uint8_t max) {
    if(value < min) return min;
    if(value > max) return max;
    return (uint8_t)value;
}

static void basic_demo_sync_feedback(BasicDemoApp* app) {
    notification_message(app->notification, &sequence_display_backlight_on);
    if(app->filled) {
        notification_message(app->notification, &sequence_set_only_blue_255);
        notification_message(app->notification, &sequence_single_vibro);
    } else {
        notification_message(app->notification, &sequence_reset_rgb);
    }
}

static void basic_demo_draw_callback(Canvas* canvas, void* context) {
    BasicDemoApp* app = context;
    char status[32];

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_frame(canvas, 0, 0, 128, 64);
    canvas_draw_str(canvas, 4, 10, "Basic demo");
    canvas_draw_line(canvas, 0, 14, 127, 14);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 4, 24, "Arrows move");
    canvas_draw_str(canvas, 4, 33, "OK toggles");
    canvas_draw_str(canvas, 4, 42, "Back exits");

    canvas_draw_frame(canvas, 86, 18, 36, 30);
    if(app->filled) {
        canvas_draw_box(canvas, app->cursor_x, app->cursor_y, 6, 6);
    } else {
        canvas_draw_frame(canvas, app->cursor_x, app->cursor_y, 6, 6);
    }

    snprintf(
        status,
        sizeof(status),
        "x:%u y:%u %s",
        (unsigned int)app->cursor_x,
        (unsigned int)app->cursor_y,
        app->filled ? "fill" : "frame");
    canvas_draw_str(canvas, 4, 58, status);
}

static void basic_demo_input_callback(InputEvent* event, void* context) {
    BasicDemoApp* app = context;

    if(event != NULL) {
        furi_message_queue_put(app->input_queue, event, FuriWaitForever);
        free(event);
    }
}

int32_t basic_demo_app(void* p) {
    UNUSED(p);

    BasicDemoApp app = {
        .input_queue = furi_message_queue_alloc(8, sizeof(InputEvent)),
        .cursor_x = 101,
        .cursor_y = 30,
        .filled = false,
        .running = true,
    };

    basic_demo_wait_for_services(&app);
    app.view_port = view_port_alloc();

    view_port_draw_callback_set(app.view_port, basic_demo_draw_callback, &app);
    view_port_input_callback_set(app.view_port, basic_demo_input_callback, &app);
    gui_add_view_port(app.gui, app.view_port, GuiLayerFullscreen);

    basic_demo_sync_feedback(&app);
    view_port_update(app.view_port);

    while(app.running) {
        InputEvent event;

        if(furi_message_queue_get(app.input_queue, &event, 100) != FuriStatusOk) {
            continue;
        }

        if(event.type != InputTypePress && event.type != InputTypeRepeat) {
            continue;
        }

        switch(event.key) {
        case InputKeyUp:
            app.cursor_y = basic_demo_clamp((int32_t)app.cursor_y - 2, 20, 41);
            break;
        case InputKeyDown:
            app.cursor_y = basic_demo_clamp((int32_t)app.cursor_y + 2, 20, 41);
            break;
        case InputKeyLeft:
            app.cursor_x = basic_demo_clamp((int32_t)app.cursor_x - 2, 88, 114);
            break;
        case InputKeyRight:
            app.cursor_x = basic_demo_clamp((int32_t)app.cursor_x + 2, 88, 114);
            break;
        case InputKeyOk:
            app.filled = !app.filled;
            basic_demo_sync_feedback(&app);
            break;
        case InputKeyBack:
            app.running = false;
            notification_message(app.notification, &sequence_reset_rgb);
            notification_message(app.notification, &sequence_display_backlight_off_delay_1000);
            break;
        default:
            break;
        }

        view_port_update(app.view_port);
    }

    furi_message_queue_free(app.input_queue);
    view_port_free(app.view_port);

    return 0;
}
