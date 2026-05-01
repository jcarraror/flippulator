#include "thread.h"
#include "string.h"
#include <limits.h>
#include <stdio.h>

#ifndef FLIPPULATOR_APP_ID
#define FLIPPULATOR_APP_ID "flippulator"
#endif

#define THREADS_MAX 1024

static uint32_t flags_g;

typedef struct FuriThreadStdout FuriThreadStdout;

struct FuriThreadStdout {
    FuriThreadStdoutWriteCallback write_callback;
    FuriString* buffer;
};

struct FuriThread {
    FuriThreadState state;
    int32_t ret;

    FuriThreadCallback callback;
    void* context;

    FuriThreadStateCallback state_callback;
    void* state_context;

    char* name;
    char* appid;

    FuriThreadPriority priority;

    TaskHandle_t task_handle;
    size_t heap_size;

    FuriThreadStdout output;

    // Keep all non-alignable byte types in one place,
    // this ensures that the size of this structure is minimal
    bool is_service;
    bool heap_trace_enabled;
    
    bool suspended;

    configSTACK_DEPTH_TYPE stack_size;
};

static FuriThread* threads[THREADS_MAX];
static unsigned int threads_i = 0;

static FuriThread* furi_thread_find_by_id(FuriThreadId thread_id) {
    for(unsigned int i = 0; i < threads_i; i++) {
        if(threads[i] != NULL && (FuriThreadId)threads[i]->task_handle == thread_id) {
            return threads[i];
        }
    }

    return NULL;
}

static void furi_thread_stdout_buffer_append(FuriThread* thread, const char* data, size_t size) {
    furi_assert(thread);
    furi_assert(data != NULL || size == 0U);

    if(size == 0U) {
        return;
    }

    FuriString* buffer = thread->output.buffer;
    const size_t buffer_size = furi_string_size(buffer);
    furi_string_reserve(buffer, buffer_size + size + 1U);

    for(size_t i = 0; i < size; i++) {
        furi_string_push_back(buffer, data[i]);
    }
}

static int32_t furi_thread_stdout_write_direct(FuriThread* thread, const char* data, size_t size) {
    furi_assert(thread);
    furi_assert(data != NULL || size == 0U);

    if(size == 0U) {
        return 0;
    }

    if(thread->output.write_callback != NULL) {
        thread->output.write_callback(data, size);
    } else {
        const size_t written = fwrite(data, 1U, size, stdout);
        fflush(stdout);
        if(written != size) {
            return -1;
        }
    }

    return 0;
}

static int32_t furi_thread_stdout_flush_internal(FuriThread* thread) {
    furi_assert(thread);

    FuriString* buffer = thread->output.buffer;
    const size_t size = furi_string_size(buffer);
    if(size == 0U) {
        return 0;
    }

    const int32_t result = furi_thread_stdout_write_direct(thread, furi_string_get_cstr(buffer), size);
    if(result == 0) {
        furi_string_reset(buffer);
    }

    return result;
}

static void furi_thread_set_suspended(FuriThreadId thread_id, bool suspended) {
    FuriThread* thread = furi_thread_find_by_id(thread_id);
    if(thread != NULL) {
        thread->suspended = suspended;
    }
}

static void furi_thread_set_state(FuriThread* thread, FuriThreadState state) {
    furi_assert(thread);
    thread->state = state;
    if(thread->state_callback) {
        thread->state_callback(state, thread->state_context);
    }
}

FuriThread* furi_thread_alloc() {
    FuriThread* thread = calloc(1, sizeof(FuriThread));
    furi_check(threads_i < THREADS_MAX);

    thread->task_handle = (TaskHandle_t)SIZE_MAX;

    thread->output.buffer = furi_string_alloc();
    thread->is_service = false;

    furi_thread_set_appid(thread, FLIPPULATOR_APP_ID);

    thread->heap_trace_enabled = false;

    threads[threads_i] = thread;
    threads_i++;

    furi_thread_set_state(thread, FuriThreadStateStopped);

    return thread;
}

FuriThread* furi_thread_alloc_ex(
    const char* name,
    uint32_t stack_size,
    FuriThreadCallback callback,
    void* context) {
    FuriThread* thread = furi_thread_alloc();
    furi_thread_set_name(thread, name);
    furi_thread_set_stack_size(thread, stack_size);
    furi_thread_set_callback(thread, callback);
    furi_thread_set_context(thread, context);
    return thread;
}

void furi_thread_free(FuriThread* thread) {
    furi_assert(thread);

    // Ensure that use join before free
    furi_assert(thread->state == FuriThreadStateStopped);

    if(thread->name) free(thread->name);
    if(thread->appid) free(thread->appid);
    furi_string_free(thread->output.buffer);

    for(unsigned int i = 0; i < threads_i; i++) {
        if(threads[i] == thread) {
            if(i + 1 < threads_i) {
                memmove(&threads[i], &threads[i + 1], (threads_i - i - 1) * sizeof(threads[0]));
            }
            threads[threads_i - 1] = NULL;
            threads_i--;
            break;
        }
    }

    free(thread);
}

void furi_thread_set_name(FuriThread* thread, const char* name) {
    furi_assert(thread);
    furi_assert(thread->state == FuriThreadStateStopped);
    if(thread->name) free(thread->name);
    thread->name = name ? strdup(name) : NULL;
}

void furi_thread_set_appid(FuriThread* thread, const char* appid) {
    furi_assert(thread);
    furi_assert(thread->state == FuriThreadStateStopped);
    if(thread->appid) free(thread->appid);
    thread->appid = appid ? strdup(appid) : NULL;
}

void furi_thread_mark_as_service(FuriThread* thread) {
    thread->is_service = true;
}

void furi_thread_set_stack_size(FuriThread* thread, size_t stack_size) {
    furi_assert(thread);
    furi_assert(thread->state == FuriThreadStateStopped);
    furi_assert(stack_size % 4 == 0);
    thread->stack_size = stack_size;
}

void furi_thread_set_callback(FuriThread* thread, FuriThreadCallback callback) {
    furi_assert(thread);
    furi_assert(thread->state == FuriThreadStateStopped);
    thread->callback = callback;
}

void furi_thread_set_context(FuriThread* thread, void* context) {
    furi_assert(thread);
    furi_assert(thread->state == FuriThreadStateStopped);
    thread->context = context;
}

void furi_thread_set_priority(FuriThread* thread, FuriThreadPriority priority) {
    furi_assert(thread);
    furi_assert(thread->state == FuriThreadStateStopped);
    furi_assert(priority >= FuriThreadPriorityIdle && priority <= FuriThreadPriorityIsr);
    thread->priority = priority;
}

void furi_thread_set_current_priority(FuriThreadPriority priority) {
    UNUSED(priority);
    // do nothing
}

FuriThreadPriority furi_thread_get_current_priority() {
    // do nothing
    return FuriThreadPriorityNone;
}

void furi_thread_set_state_callback(FuriThread* thread, FuriThreadStateCallback callback) {
    furi_assert(thread);
    furi_assert(thread->state == FuriThreadStateStopped);
    thread->state_callback = callback;
}

void furi_thread_set_state_context(FuriThread* thread, void* context) {
    furi_assert(thread);
    furi_assert(thread->state == FuriThreadStateStopped);
    thread->state_context = context;
}

FuriThreadState furi_thread_get_state(FuriThread* thread) {
    furi_assert(thread);
    return thread->state;
}

static void* furi_thread_body(void* ctx_) {
    FuriThread* thread = ctx_;
    furi_thread_set_state(thread, FuriThreadStateRunning);
    thread->ret = thread->callback(thread->context);
    furi_thread_set_state(thread, FuriThreadStateStopped);
    return NULL;
}

void furi_thread_start(FuriThread* thread) {
    furi_assert(thread);
    furi_assert(thread->callback);
    furi_assert(thread->state == FuriThreadStateStopped);
    furi_assert(thread->stack_size > 0 && thread->stack_size < (UINT16_MAX * sizeof(StackType_t)));

    furi_thread_set_state(thread, FuriThreadStateStarting);

    pthread_t thread_id;
    pthread_create(&thread_id, NULL, furi_thread_body, thread);
    thread->task_handle = (TaskHandle_t)thread_id;

    furi_check(thread->task_handle);
}

void furi_thread_cleanup_tcb_event(TaskHandle_t task) {
    UNUSED(task);
    // do nothing
}

bool furi_thread_join(FuriThread* thread) {
    // printf("0x%lx\n", (unsigned long)thread->task_handle);
    int32_t res = (int32_t)pthread_join((pthread_t)thread->task_handle, NULL);
    return res == 0;
}

FuriThreadId furi_thread_get_id(FuriThread* thread) {
    furi_assert(thread);
    return (FuriThreadId)thread->task_handle;
}

void furi_thread_enable_heap_trace(FuriThread* thread) {
    furi_assert(thread);
    furi_assert(thread->state == FuriThreadStateStopped);
    thread->heap_trace_enabled = true;
}

void furi_thread_disable_heap_trace(FuriThread* thread) {
    furi_assert(thread);
    furi_assert(thread->state == FuriThreadStateStopped);
    thread->heap_trace_enabled = false;
}

size_t furi_thread_get_heap_size(FuriThread* thread) {
    furi_assert(thread);
    furi_assert(thread->heap_trace_enabled == true);
    return thread->heap_size;
}

int32_t furi_thread_get_return_code(FuriThread* thread) {
    furi_assert(thread);
    furi_assert(thread->state == FuriThreadStateStopped);
    return thread->ret;
}

FuriThreadId furi_thread_get_current_id() {
    return (FuriThreadId)pthread_self();
}

FuriThread* furi_thread_get_current() {
    return furi_thread_find_by_id(furi_thread_get_current_id());
}

void furi_thread_yield() {
    // do nothing
}

/* Limits */
#define MAX_BITS_TASK_NOTIFY 31U
#define MAX_BITS_EVENT_GROUPS 24U

#define THREAD_FLAGS_INVALID_BITS (~((1UL << MAX_BITS_TASK_NOTIFY) - 1U))
#define EVENT_FLAGS_INVALID_BITS (~((1UL << MAX_BITS_EVENT_GROUPS) - 1U))

uint32_t furi_thread_flags_set(FuriThreadId thread_id, uint32_t flags) {
    UNUSED(thread_id);
    flags_g = flags;
    return flags;
}

uint32_t furi_thread_flags_clear(uint32_t flags) {
    flags_g = flags;
    return flags;
}

uint32_t furi_thread_flags_get(void) {
    return flags_g;
}

uint32_t furi_thread_flags_wait(uint32_t flags, uint32_t options, uint32_t timeout) {
    UNUSED(flags);
    UNUSED(options);
    UNUSED(timeout);
    return flags_g;
}

uint32_t furi_thread_enumerate(FuriThreadId* thread_array, uint32_t array_items) {
    uint32_t count = 0;

    for(unsigned int i = 0; i < threads_i && count < array_items; i++) {
        thread_array[count++] = threads[i];
    }

    return count;
}

const char* furi_thread_get_name(FuriThreadId thread_id) {
    FuriThread* thread = furi_thread_find_by_id(thread_id);
    return thread != NULL ? thread->name : NULL;
}

const char* furi_thread_get_appid(FuriThreadId thread_id) {
    FuriThread* thread = furi_thread_find_by_id(thread_id);
    return thread != NULL ? thread->appid : NULL;
}

uint32_t furi_thread_get_stack_space(FuriThreadId thread_id) {
    FuriThread* thread = furi_thread_find_by_id(thread_id);
    return thread != NULL ? thread->stack_size : 0;
}

void furi_thread_set_stdout_callback(FuriThreadStdoutWriteCallback callback) {
    FuriThread* thread = furi_thread_get_current();
    furi_assert(thread);
    furi_thread_stdout_flush_internal(thread);
    thread->output.write_callback = callback;
}

FuriThreadStdoutWriteCallback furi_thread_get_stdout_callback() {
    FuriThread* thread = furi_thread_get_current();
    furi_assert(thread);
    return thread->output.write_callback;
}

size_t furi_thread_stdout_write(const char* data, size_t size) {
    FuriThread* thread = furi_thread_get_current();
    furi_assert(thread);

    if(data == NULL || size == 0U) {
        return furi_thread_stdout_flush_internal(thread) == 0 ? 0U : 0U;
    }

    size_t offset = 0U;
    while(offset < size) {
        const void* newline_ptr = memchr(data + offset, '\n', size - offset);
        if(newline_ptr == NULL) {
            furi_thread_stdout_buffer_append(thread, data + offset, size - offset);
            break;
        }

        const size_t chunk_size = (const char*)newline_ptr - (data + offset) + 1U;
        if(furi_string_size(thread->output.buffer) == 0U) {
            furi_thread_stdout_write_direct(thread, data + offset, chunk_size);
        } else {
            furi_thread_stdout_buffer_append(thread, data + offset, chunk_size);
            furi_thread_stdout_flush_internal(thread);
        }

        offset += chunk_size;
    }

    return size;
}

int32_t furi_thread_stdout_flush() {
    FuriThread* thread = furi_thread_get_current();
    furi_assert(thread);
    return furi_thread_stdout_flush_internal(thread);
}

void furi_thread_suspend(FuriThreadId thread_id) {
    /* Track suspend state for simulator compatibility.
       Forcibly suspending pthreads is non-portable and unsafe. */
    furi_thread_set_suspended(thread_id, true);
}

void furi_thread_resume(FuriThreadId thread_id) {
    furi_thread_set_suspended(thread_id, false);
}

bool furi_thread_is_suspended(FuriThreadId thread_id) {
    FuriThread* thread = furi_thread_find_by_id(thread_id);
    return thread != NULL ? thread->suspended : false;
}
