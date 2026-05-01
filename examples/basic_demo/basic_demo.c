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
    uint8_t dot_x;
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
    {.title = "Success", .sequence = &sequence_success, .hint = "sound + green"},
    {.title = "Error", .sequence = &sequence_error, .hint = "sound + red"},
    {.title = "Vibro", .sequence = &sequence_single_vibro, .hint = "haptic pulse"},
    {.title = "Alert", .sequence = &sequence_audiovisual_alert, .hint = "audio + blink"},
};

#define BASIC_DEMO_WIDGET_PRESET_COUNT \
    (sizeof(basic_demo_widget_presets) / sizeof(basic_demo_widget_presets[0]))

static void basic_demo_widget_button_callback(
    GuiButtonType result,
    InputType type,
    void* context);

static void basic_demo_notify(BasicDemoApp* app, const NotificationSequence* sequence) {
    notification_message(app->notification, &sequence_display_backlight_on);
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
    snprintf(hint_line, sizeof(hint_line), "Effect: %s", preset->hint);
    snprintf(count_line, sizeof(count_line), "Triggered: %u", (unsigned)app->widget_fired_count);

    widget_reset(app->widget);
   
    widget_add_rect_element(app->widget, 0, 0, 128, 13, 0, true);
    widget_add_string_element(app->widget, 64, 10, AlignCenter, AlignBottom, FontPrimary, "Widget View");
    widget_add_rect_element(app->widget, 6, 14, 116, 36, 2, false);
    widget_add_string_element(
        app->widget, 64, 24, AlignCenter, AlignBottom, FontPrimary, selected_line);
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
        basic_demo_notify(app, preset->sequence);
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

    canvas_clear(canvas);
    canvas_draw_frame(canvas, 0, 0, 128, 64);
    canvas_draw_box(canvas, 0, 0, 128, 13);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignBottom, "Canvas View");
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_line(canvas, 0, 54, 127, 54);
    elements_button_center(canvas, "Pause");
    elements_button_right(canvas, "Back");
    canvas_draw_frame(canvas, 6, 18, 116, 32);

    canvas_draw_frame(canvas, 14, 24, 16, 10);
    canvas_draw_box(canvas, 36, 24, 16, 10);
    canvas_draw_rframe(canvas, 58, 24, 16, 10, 2);
    canvas_draw_rbox(canvas, 80, 24, 16, 10, 2);
    canvas_draw_circle(canvas, 108, 29, 5);

    canvas_draw_line(canvas, 14, 42, 48, 42);
    canvas_draw_triangle(canvas, 68, 42, 12, 10, CanvasDirectionBottomToTop);
    canvas_draw_disc(canvas, canvas_model->dot_x, 42, 1);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas,
        104,
        44,
        AlignCenter,
        AlignBottom,
        canvas_model->paused ? "paused" : "live");
}

static bool basic_demo_canvas_input(InputEvent* event, void* context) {
    BasicDemoApp* app = context;
    if(event->type == InputTypePress && event->key == InputKeyOk) {
        with_view_model(
            app->canvas_view,
            BasicDemoCanvasModel * model,
            { model->paused = !model->paused; },
            true);
        basic_demo_notify(app, &sequence_single_vibro);
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
                model->dot_x++;
                if(model->dot_x > 46U) {
                    model->dot_x = 18U;
                }
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
            model->dot_x = 18U;
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
