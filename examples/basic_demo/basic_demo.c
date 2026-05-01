#include <furi.h>

#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/elements.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <stdio.h>

typedef enum {
    BasicDemoViewMenu,
    BasicDemoViewWidget,
    BasicDemoViewCanvas,
    BasicDemoViewSignals,
} BasicDemoView;

typedef enum {
    BasicDemoMenuWidget,
    BasicDemoMenuCanvas,
    BasicDemoMenuSignals,
    BasicDemoMenuExit,
} BasicDemoMenuIndex;

typedef enum {
    BasicDemoSignalSuccess,
    BasicDemoSignalError,
    BasicDemoSignalVibro,
    BasicDemoSignalAlert,
} BasicDemoSignalIndex;

typedef struct {
    uint8_t phase;
    uint8_t speed;
    uint8_t amplitude;
    uint8_t wave_mode;
    bool paused;
} BasicDemoCanvasModel;

typedef struct {
    const char* title;
    const NotificationSequence* sequence;
    const char* hint;
} BasicDemoWidgetPreset;

typedef struct {
    ViewDispatcher* view_dispatcher;
    Submenu* menu;
    Widget* widget;
    View* canvas_view;
    Submenu* signals;
    NotificationApp* notification;
    uint8_t widget_selected;
    uint16_t widget_fired_count;
} BasicDemoApp;

static const BasicDemoWidgetPreset basic_demo_widget_presets[] = {
    {.title = "Success", .sequence = &sequence_success, .hint = "green chime"},
    {.title = "Error", .sequence = &sequence_error, .hint = "red buzz"},
    {.title = "Vibro", .sequence = &sequence_single_vibro, .hint = "vibro pulse"},
    {.title = "Alert", .sequence = &sequence_audiovisual_alert, .hint = "blink alert"},
};

#define BASIC_DEMO_WIDGET_PRESET_COUNT \
    (sizeof(basic_demo_widget_presets) / sizeof(basic_demo_widget_presets[0]))

static void basic_demo_widget_button_callback(
    GuiButtonType result,
    InputType type,
    void* context);

static const char* basic_demo_wave_name(uint8_t wave_mode) {
    if(wave_mode == 0U) {
        return "TRI";
    } else if(wave_mode == 1U) {
        return "SQR";
    }
    return "SAW";
}

static int8_t basic_demo_wave_sample(const BasicDemoCanvasModel* model, uint8_t x) {
    const uint8_t period = 64U;
    const uint8_t t = (uint8_t)((x + model->phase) % period);
    const int8_t amp = (int8_t)model->amplitude;

    if(model->wave_mode == 0U) {
        /* Triangle wave in range [-amp, amp]. */
        if(t < (period / 2U)) {
            return (int8_t)(-amp + ((int16_t)t * (2 * amp)) / (period / 2U));
        }
        return (int8_t)(amp - ((int16_t)(t - (period / 2U)) * (2 * amp)) / (period / 2U));
    } else if(model->wave_mode == 1U) {
        return (t < (period / 2U)) ? amp : (int8_t)-amp;
    }

    /* Saw wave in range [-amp, amp]. */
    return (int8_t)(-amp + ((int16_t)t * (2 * amp)) / (period - 1U));
}

static void basic_demo_notify(BasicDemoApp* app, const NotificationSequence* sequence) {
    notification_message(app->notification, &sequence_display_backlight_on);
    notification_message(app->notification, sequence);
}

static void basic_demo_notify_replace_led(BasicDemoApp* app, const NotificationSequence* sequence) {
    notification_message(app->notification, &sequence_display_backlight_on);
    notification_message(app->notification, &sequence_reset_rgb);
    notification_message(app->notification, sequence);
}

static void basic_demo_widget_refresh(BasicDemoApp* app) {
    char selected_line[22];
    char hint_line[22];
    char count_line[22];

    const BasicDemoWidgetPreset* preset = &basic_demo_widget_presets[app->widget_selected];

    snprintf(
        selected_line,
        sizeof(selected_line),
        "%u/%u: %s",
        (unsigned)(app->widget_selected + 1U),
        (unsigned)BASIC_DEMO_WIDGET_PRESET_COUNT,
        preset->title);
    snprintf(hint_line, sizeof(hint_line), "fx: %s", preset->hint);
    snprintf(count_line, sizeof(count_line), "Triggered: %u", (unsigned)app->widget_fired_count);

    widget_reset(app->widget);
   
    widget_add_rect_element(app->widget, 0, 0, 128, 13, 0, true);
    widget_add_string_element(app->widget, 64, 10, AlignCenter, AlignBottom, FontPrimary, "Widget View");
    widget_add_rect_element(app->widget, 6, 14, 116, 36, 2, false);
    widget_add_string_element(
        app->widget, 64, 24, AlignCenter, AlignBottom, FontSecondary, selected_line);
    widget_add_string_element(
        app->widget, 64, 35, AlignCenter, AlignBottom, FontSecondary, hint_line);
    widget_add_string_element(
        app->widget, 64, 46, AlignCenter, AlignBottom, FontSecondary, count_line);
    widget_add_button_element(
        app->widget, GuiButtonTypeLeft, "Prev", basic_demo_widget_button_callback, app);
    widget_add_button_element(
        app->widget, GuiButtonTypeCenter, "Send", basic_demo_widget_button_callback, app);
    widget_add_button_element(
        app->widget, GuiButtonTypeRight, "Next", basic_demo_widget_button_callback, app);
}

static uint32_t basic_demo_back_to_menu(void* context) {
    UNUSED(context);
    return BasicDemoViewMenu;
}

static bool basic_demo_navigation_exit(void* context) {
    BasicDemoApp* app = context;
    view_dispatcher_stop(app->view_dispatcher);
    return true;
}

static void basic_demo_menu_callback(void* context, uint32_t index) {
    BasicDemoApp* app = context;

    if(index == BasicDemoMenuExit) {
        view_dispatcher_stop(app->view_dispatcher);
        return;
    }

    if(index == BasicDemoMenuWidget) {
        view_dispatcher_switch_to_view(app->view_dispatcher, BasicDemoViewWidget);
    } else if(index == BasicDemoMenuCanvas) {
        view_dispatcher_switch_to_view(app->view_dispatcher, BasicDemoViewCanvas);
    } else if(index == BasicDemoMenuSignals) {
        view_dispatcher_switch_to_view(app->view_dispatcher, BasicDemoViewSignals);
    }
}

static void basic_demo_widget_button_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    BasicDemoApp* app = context;
    if(type != InputTypeShort) {
        return;
    }

    if(result == GuiButtonTypeLeft) {
        if(app->widget_selected == 0U) {
            app->widget_selected = (uint8_t)(BASIC_DEMO_WIDGET_PRESET_COUNT - 1U);
        } else {
            app->widget_selected--;
        }

        basic_demo_notify(app, &sequence_single_vibro);
        basic_demo_widget_refresh(app);
    } else if(result == GuiButtonTypeCenter) {
        const BasicDemoWidgetPreset* preset = &basic_demo_widget_presets[app->widget_selected];
        basic_demo_notify_replace_led(app, preset->sequence);
        app->widget_fired_count++;
        basic_demo_widget_refresh(app);
    } else if(result == GuiButtonTypeRight) {
        app->widget_selected = (uint8_t)((app->widget_selected + 1U) % BASIC_DEMO_WIDGET_PRESET_COUNT);

        basic_demo_notify(app, &sequence_single_vibro);
        basic_demo_widget_refresh(app);
    }
}

static void basic_demo_canvas_draw(Canvas* canvas, void* model) {
    BasicDemoCanvasModel* canvas_model = model;
    char info_line[22];
    const uint8_t graph_x = 8U;
    const uint8_t graph_y = 24U;
    const uint8_t graph_w = 112U;
    const uint8_t graph_h = 16U;
    const uint8_t mid_y = (uint8_t)(graph_y + (graph_h / 2U));

    canvas_clear(canvas);
    canvas_draw_frame(canvas, 0, 0, 128, 64);
    canvas_draw_box(canvas, 0, 0, 128, 13);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignBottom, "Canvas Lab");
    canvas_set_color(canvas, ColorBlack);

    snprintf(
        info_line,
        sizeof(info_line),
        "%s s%u %s",
        basic_demo_wave_name(canvas_model->wave_mode),
        (unsigned)canvas_model->speed,
        canvas_model->paused ? "P" : "L");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 21, AlignCenter, AlignBottom, info_line);

    canvas_draw_frame(canvas, graph_x, graph_y, graph_w, graph_h);
    canvas_draw_line(canvas, graph_x + 1U, mid_y, graph_x + graph_w - 2U, mid_y);

    /* Draw waveform as connected line segments across the graph area. */
    int8_t prev = basic_demo_wave_sample(canvas_model, 0U);
    for(uint8_t px = 1U; px < (uint8_t)(graph_w - 2U); px++) {
        int8_t curr = basic_demo_wave_sample(canvas_model, px);
        canvas_draw_line(
            canvas,
            (uint8_t)(graph_x + px),
            (uint8_t)((int16_t)mid_y - prev),
            (uint8_t)(graph_x + px + 1U),
            (uint8_t)((int16_t)mid_y - curr));
        prev = curr;
    }

    canvas_draw_disc(canvas, (uint8_t)(graph_x + graph_w - 3U), (uint8_t)((int16_t)mid_y - prev), 1U);

    canvas_draw_str_aligned(canvas, 64, 48, AlignCenter, AlignBottom, "U/D speed");

    canvas_draw_line(canvas, 0, 54, 127, 54);
    elements_button_left(canvas, "Wave-");
    elements_button_center(canvas, canvas_model->paused ? "Run" : "Pause");
    elements_button_right(canvas, "Wave+");
}

static bool basic_demo_canvas_input(InputEvent* event, void* context) {
    BasicDemoApp* app = context;
    if(event->type != InputTypePress && event->type != InputTypeRepeat) {
        return false;
    }

    if(event->key == InputKeyOk && event->type == InputTypePress) {
        with_view_model(
            app->canvas_view,
            BasicDemoCanvasModel * model,
            { model->paused = !model->paused; },
            true);
        basic_demo_notify(app, &sequence_single_vibro);
        return true;
    } else if(event->key == InputKeyLeft && event->type == InputTypePress) {
        with_view_model(
            app->canvas_view,
            BasicDemoCanvasModel * model,
            {
                if(model->wave_mode == 0U) {
                    model->wave_mode = 2U;
                } else {
                    model->wave_mode--;
                }
            },
            true);
        basic_demo_notify(app, &sequence_single_vibro);
        return true;
    } else if(event->key == InputKeyRight && event->type == InputTypePress) {
        with_view_model(
            app->canvas_view,
            BasicDemoCanvasModel * model,
            { model->wave_mode = (uint8_t)((model->wave_mode + 1U) % 3U); },
            true);
        basic_demo_notify(app, &sequence_single_vibro);
        return true;
    } else if(event->key == InputKeyUp) {
        with_view_model(
            app->canvas_view,
            BasicDemoCanvasModel * model,
            {
                if(model->speed < 8U) {
                    model->speed++;
                }
                model->amplitude = (uint8_t)(1U + model->speed);
            },
            true);
        if(event->type == InputTypePress) {
            basic_demo_notify(app, &sequence_single_vibro);
        }
        return true;
    } else if(event->key == InputKeyDown) {
        with_view_model(
            app->canvas_view,
            BasicDemoCanvasModel * model,
            {
                if(model->speed > 1U) {
                    model->speed--;
                }
                model->amplitude = (uint8_t)(1U + model->speed);
            },
            true);
        if(event->type == InputTypePress) {
            basic_demo_notify(app, &sequence_single_vibro);
        }
        return true;
    }

    return false;
}

static void basic_demo_tick(void* context) {
    BasicDemoApp* app = context;
    if(app->canvas_view == NULL) {
        return;
    }

    with_view_model(
        app->canvas_view,
        BasicDemoCanvasModel * model,
        {
            if(!model->paused) {
                model->phase = (uint8_t)((model->phase + model->speed) % 64U);
            }
        },
        true);
}

static void basic_demo_signal_callback(void* context, uint32_t index) {
    BasicDemoApp* app = context;
    if(index == BasicDemoSignalSuccess) {
        basic_demo_notify(app, &sequence_success);
    } else if(index == BasicDemoSignalError) {
        basic_demo_notify(app, &sequence_error);
    } else if(index == BasicDemoSignalVibro) {
        basic_demo_notify(app, &sequence_single_vibro);
    } else if(index == BasicDemoSignalAlert) {
        basic_demo_notify(app, &sequence_audiovisual_alert);
    }
}

static void basic_demo_setup_menu(BasicDemoApp* app) {
    app->menu = submenu_alloc();
    submenu_set_header(app->menu, "API Demo");
    submenu_add_item(
        app->menu, "Widget screen", BasicDemoMenuWidget, basic_demo_menu_callback, app);
    submenu_add_item(
        app->menu, "Canvas screen", BasicDemoMenuCanvas, basic_demo_menu_callback, app);
    submenu_add_item(
        app->menu, "Signal menu", BasicDemoMenuSignals, basic_demo_menu_callback, app);
    submenu_add_item(app->menu, "Exit", BasicDemoMenuExit, basic_demo_menu_callback, app);
}

static void basic_demo_setup_widget(BasicDemoApp* app) {
    app->widget = widget_alloc();
    View* view = widget_get_view(app->widget);
    view_set_previous_callback(view, basic_demo_back_to_menu);

    app->widget_selected = 0U;
    app->widget_fired_count = 0U;
    basic_demo_widget_refresh(app);
}

static void basic_demo_setup_canvas(BasicDemoApp* app) {
    app->canvas_view = view_alloc();
    view_set_context(app->canvas_view, app);
    view_set_draw_callback(app->canvas_view, basic_demo_canvas_draw);
    view_set_input_callback(app->canvas_view, basic_demo_canvas_input);
    view_set_previous_callback(app->canvas_view, basic_demo_back_to_menu);
    view_allocate_model(app->canvas_view, ViewModelTypeLockFree, sizeof(BasicDemoCanvasModel));

    with_view_model(
        app->canvas_view,
        BasicDemoCanvasModel * model,
        {
            model->phase = 0U;
            model->speed = 2U;
            model->amplitude = 3U;
            model->wave_mode = 0U;
            model->paused = false;
        },
        false);
}

static void basic_demo_setup_signals(BasicDemoApp* app) {
    app->signals = submenu_alloc();
    submenu_set_header(app->signals, "Signals");
    submenu_add_item(
        app->signals, "Success", BasicDemoSignalSuccess, basic_demo_signal_callback, app);
    submenu_add_item(app->signals, "Error", BasicDemoSignalError, basic_demo_signal_callback, app);
    submenu_add_item(app->signals, "Vibro", BasicDemoSignalVibro, basic_demo_signal_callback, app);
    submenu_add_item(app->signals, "Alert", BasicDemoSignalAlert, basic_demo_signal_callback, app);
    view_set_previous_callback(submenu_get_view(app->signals), basic_demo_back_to_menu);
}

int32_t basic_demo_app(void* p) {
    UNUSED(p);

    BasicDemoApp app = {0};

    while(app.notification == NULL) {
        app.notification = furi_record_open(RECORD_NOTIFICATION);
        if(app.notification == NULL) {
            furi_delay_ms(10);
        }
    }

    Gui* gui = NULL;
    while(gui == NULL) {
        gui = furi_record_open(RECORD_GUI);
        if(gui == NULL) {
            furi_delay_ms(10);
        }
    }

    app.view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app.view_dispatcher, &app);
    view_dispatcher_set_navigation_event_callback(
        app.view_dispatcher, basic_demo_navigation_exit);
    view_dispatcher_set_tick_event_callback(app.view_dispatcher, basic_demo_tick, 120);

    basic_demo_setup_menu(&app);
    basic_demo_setup_widget(&app);
    basic_demo_setup_canvas(&app);
    basic_demo_setup_signals(&app);

    view_dispatcher_add_view(app.view_dispatcher, BasicDemoViewMenu, submenu_get_view(app.menu));
    view_dispatcher_add_view(app.view_dispatcher, BasicDemoViewWidget, widget_get_view(app.widget));
    view_dispatcher_add_view(app.view_dispatcher, BasicDemoViewCanvas, app.canvas_view);
    view_dispatcher_add_view(
        app.view_dispatcher, BasicDemoViewSignals, submenu_get_view(app.signals));
    view_dispatcher_attach_to_gui(app.view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_switch_to_view(app.view_dispatcher, BasicDemoViewMenu);
    view_dispatcher_run(app.view_dispatcher);

    notification_message(app.notification, &sequence_reset_rgb);
    notification_message(app.notification, &sequence_reset_vibro);
    notification_message(app.notification, &sequence_display_backlight_off_delay_1000);

    view_dispatcher_remove_view(app.view_dispatcher, BasicDemoViewSignals);
    view_dispatcher_remove_view(app.view_dispatcher, BasicDemoViewCanvas);
    view_dispatcher_remove_view(app.view_dispatcher, BasicDemoViewWidget);
    view_dispatcher_remove_view(app.view_dispatcher, BasicDemoViewMenu);
    submenu_free(app.signals);
    view_free(app.canvas_view);
    widget_free(app.widget);
    submenu_free(app.menu);
    view_dispatcher_free(app.view_dispatcher);

    return 0;
}
