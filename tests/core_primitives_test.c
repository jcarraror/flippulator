#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "core/base.h"
#include "core/event_flag.h"
#include "core/kernel.h"
#include "core/mutex.h"
#include "core/semaphore.h"
#include "core/thread.h"

FuriThreadId furi_thread_get_current_id() {
    return (FuriThreadId)pthread_self();
}

void crash(uint8_t code, const char* msg) {
    fprintf(stderr, "unexpected crash %u: %s\n", code, msg != NULL ? msg : "<null>");
    abort();
}

typedef struct {
    FuriMutex* mutex;
    FuriStatus status;
} MutexWorkerCtx;

static void* mutex_timeout_worker(void* context) {
    MutexWorkerCtx* ctx = context;
    ctx->status = furi_mutex_acquire(ctx->mutex, 25);
    return NULL;
}

static void* mutex_success_worker(void* context) {
    MutexWorkerCtx* ctx = context;
    ctx->status = furi_mutex_acquire(ctx->mutex, FuriWaitForever);
    if(ctx->status == FuriStatusOk) {
        ctx->status = furi_mutex_release(ctx->mutex);
    }
    return NULL;
}

static void test_mutexes(void) {
    FuriMutex* recursive = furi_mutex_alloc(FuriMutexTypeRecursive);
    assert(recursive != NULL);
    assert(furi_mutex_acquire(recursive, 0) == FuriStatusOk);
    assert(furi_mutex_acquire(recursive, 0) == FuriStatusOk);
    assert(furi_mutex_release(recursive) == FuriStatusOk);
    assert(furi_mutex_release(recursive) == FuriStatusOk);
    furi_mutex_free(recursive);

    FuriMutex* normal = furi_mutex_alloc(FuriMutexTypeNormal);
    assert(normal != NULL);
    assert(furi_mutex_acquire(normal, 0) == FuriStatusOk);

    MutexWorkerCtx timeout_ctx = {.mutex = normal, .status = FuriStatusReserved};
    pthread_t timeout_thread;
    pthread_create(&timeout_thread, NULL, mutex_timeout_worker, &timeout_ctx);
    pthread_join(timeout_thread, NULL);
    assert(timeout_ctx.status == FuriStatusErrorTimeout);

    MutexWorkerCtx success_ctx = {.mutex = normal, .status = FuriStatusReserved};
    pthread_t success_thread;
    pthread_create(&success_thread, NULL, mutex_success_worker, &success_ctx);
    furi_delay_ms(10);
    assert(furi_mutex_release(normal) == FuriStatusOk);
    pthread_join(success_thread, NULL);
    assert(success_ctx.status == FuriStatusOk);

    furi_mutex_free(normal);
}

typedef struct {
    FuriSemaphore* semaphore;
    FuriStatus status;
} SemaphoreWorkerCtx;

static void* semaphore_worker(void* context) {
    SemaphoreWorkerCtx* ctx = context;
    ctx->status = furi_semaphore_acquire(ctx->semaphore, FuriWaitForever);
    return NULL;
}

static void test_semaphores(void) {
    FuriSemaphore* semaphore = furi_semaphore_alloc(2, 0);
    assert(semaphore != NULL);
    assert(furi_semaphore_get_count(semaphore) == 0U);
    assert(furi_semaphore_acquire(semaphore, 0) == FuriStatusErrorTimeout);

    assert(furi_semaphore_release(semaphore) == FuriStatusOk);
    assert(furi_semaphore_release(semaphore) == FuriStatusOk);
    assert(furi_semaphore_release(semaphore) == FuriStatusErrorResource);
    assert(furi_semaphore_get_count(semaphore) == 2U);

    assert(furi_semaphore_acquire(semaphore, 0) == FuriStatusOk);
    assert(furi_semaphore_acquire(semaphore, 0) == FuriStatusOk);
    assert(furi_semaphore_acquire(semaphore, 0) == FuriStatusErrorTimeout);
    assert(furi_semaphore_get_count(semaphore) == 0U);

    SemaphoreWorkerCtx worker = {.semaphore = semaphore, .status = FuriStatusReserved};
    pthread_t worker_thread;
    pthread_create(&worker_thread, NULL, semaphore_worker, &worker);
    furi_delay_ms(10);
    assert(furi_semaphore_release(semaphore) == FuriStatusOk);
    pthread_join(worker_thread, NULL);
    assert(worker.status == FuriStatusOk);
    assert(furi_semaphore_get_count(semaphore) == 0U);

    furi_semaphore_free(semaphore);
}

typedef struct {
    FuriEventFlag* event;
    uint32_t status;
} EventWorkerCtx;

static void* event_wait_worker(void* context) {
    EventWorkerCtx* ctx = context;
    ctx->status = furi_event_flag_wait(ctx->event, 0x4U, FuriFlagWaitAny, FuriWaitForever);
    return NULL;
}

static void test_event_flags(void) {
    FuriEventFlag* event = furi_event_flag_alloc();
    assert(event != NULL);
    assert(furi_event_flag_get(event) == 0U);

    assert(furi_event_flag_set(event, 0x1U) == 0x1U);
    assert(furi_event_flag_set(event, 0x2U) == 0x3U);
    assert(furi_event_flag_wait(event, 0x2U, FuriFlagWaitAny, 0) == 0x2U);
    assert(furi_event_flag_get(event) == 0x1U);

    assert(furi_event_flag_set(event, 0x2U) == 0x3U);
    assert(furi_event_flag_wait(event, 0x3U, FuriFlagWaitAll | FuriFlagNoClear, 0) == 0x3U);
    assert(furi_event_flag_get(event) == 0x3U);
    assert(furi_event_flag_clear(event, 0x1U) == 0x2U);
    assert(furi_event_flag_wait(event, 0x4U, FuriFlagWaitAny, 0) == FuriFlagErrorTimeout);

    EventWorkerCtx worker = {.event = event, .status = FuriFlagErrorUnknown};
    pthread_t worker_thread;
    pthread_create(&worker_thread, NULL, event_wait_worker, &worker);
    furi_delay_ms(10);
    assert(furi_event_flag_set(event, 0x4U) == 0x6U);
    pthread_join(worker_thread, NULL);
    assert(worker.status == 0x4U);
    assert(furi_event_flag_get(event) == 0x2U);

    furi_event_flag_free(event);
}

int main(void) {
    test_mutexes();
    test_semaphores();
    test_event_flags();
    puts("core primitive tests passed");
    return 0;
}