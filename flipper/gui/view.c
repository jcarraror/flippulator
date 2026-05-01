#include "view.h"

#include <furi.h>

struct View {
    ViewDrawCallback draw_callback;
    ViewInputCallback input_callback;
    ViewCustomCallback custom_callback;
    ViewNavigationCallback previous_callback;
    ViewCallback enter_callback;
    ViewCallback exit_callback;
    ViewUpdateCallback update_callback;
    void* update_context;
    void* context;
    void* model;
    size_t model_size;
    ViewModelType model_type;
    FuriMutex* model_mutex;
    bool model_locked;
    ViewOrientation orientation;
};

View* view_alloc(void) {
    return calloc(1, sizeof(View));
}

void view_free_model(View* view) {
    furi_assert(view);
    if(view->model_mutex != NULL) {
        furi_mutex_free(view->model_mutex);
        view->model_mutex = NULL;
    }
    free(view->model);
    view->model = NULL;
    view->model_size = 0U;
    view->model_type = ViewModelTypeNone;
    view->model_locked = false;
}

void view_free(View* view) {
    furi_assert(view);
    view_free_model(view);
    free(view);
}

void view_tie_icon_animation(View* view, IconAnimation* icon_animation) {
    UNUSED(view);
    UNUSED(icon_animation);
}

void view_set_draw_callback(View* view, ViewDrawCallback callback) {
    view->draw_callback = callback;
}

void view_set_input_callback(View* view, ViewInputCallback callback) {
    view->input_callback = callback;
}

void view_set_custom_callback(View* view, ViewCustomCallback callback) {
    view->custom_callback = callback;
}

void view_set_previous_callback(View* view, ViewNavigationCallback callback) {
    view->previous_callback = callback;
}

void view_set_enter_callback(View* view, ViewCallback callback) {
    view->enter_callback = callback;
}

void view_set_exit_callback(View* view, ViewCallback callback) {
    view->exit_callback = callback;
}

void view_set_update_callback(View* view, ViewUpdateCallback callback) {
    view->update_callback = callback;
}

void view_set_update_callback_context(View* view, void* context) {
    view->update_context = context;
}

void view_set_context(View* view, void* context) {
    view->context = context;
}

void view_set_orientation(View* view, ViewOrientation orientation) {
    view->orientation = orientation;
}

void view_allocate_model(View* view, ViewModelType type, size_t size) {
    furi_assert(view);
    view_free_model(view);
    if(size == 0U || type == ViewModelTypeNone) {
        return;
    }

    view->model = calloc(1, size);
    view->model_size = size;
    view->model_type = type;
    if(type == ViewModelTypeLocking) {
        view->model_mutex = furi_mutex_alloc(FuriMutexTypeRecursive);
    }
}

void* view_get_model(View* view) {
    furi_assert(view);
    if(view->model_type == ViewModelTypeLocking && view->model_mutex != NULL) {
        furi_check(furi_mutex_acquire(view->model_mutex, FuriWaitForever) == FuriStatusOk);
        view->model_locked = true;
    }
    return view->model;
}

void view_commit_model(View* view, bool update) {
    furi_assert(view);
    if(view->model_locked && view->model_mutex != NULL) {
        furi_check(furi_mutex_release(view->model_mutex) == FuriStatusOk);
        view->model_locked = false;
    }

    if(update && view->update_callback != NULL) {
        view->update_callback(view, view->update_context);
    }
}

void view_draw(View* view, Canvas* canvas) {
    furi_assert(view);
    if(view->draw_callback == NULL) {
        return;
    }

    canvas_set_orientation(canvas, (CanvasOrientation)view->orientation);

    void* model = view->model;
    if(view->model_type == ViewModelTypeLocking && view->model_mutex != NULL) {
        furi_check(furi_mutex_acquire(view->model_mutex, FuriWaitForever) == FuriStatusOk);
        view->draw_callback(canvas, model);
        furi_check(furi_mutex_release(view->model_mutex) == FuriStatusOk);
    } else {
        view->draw_callback(canvas, model);
    }
}

bool view_input(View* view, InputEvent* event) {
    furi_assert(view);
    if(view->input_callback == NULL) {
        return false;
    }
    return view->input_callback(event, view->context);
}

bool view_custom(View* view, uint32_t event) {
    furi_assert(view);
    if(view->custom_callback == NULL) {
        return false;
    }
    return view->custom_callback(event, view->context);
}

uint32_t view_previous(View* view) {
    furi_assert(view);
    if(view->previous_callback == NULL) {
        return VIEW_NONE;
    }
    return view->previous_callback(view->context);
}

void view_enter(View* view) {
    furi_assert(view);
    if(view->enter_callback != NULL) {
        view->enter_callback(view->context);
    }
}

void view_exit(View* view) {
    furi_assert(view);
    if(view->exit_callback != NULL) {
        view->exit_callback(view->context);
    }
}
