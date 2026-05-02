#include "semaphore.h"
#include "check.h"
#include "common_defines.h"
#include <errno.h>
#include <string.h>
#include <time.h>

static void furi_semaphore_timeout_to_abs(uint32_t timeout, struct timespec* ts) {
    clock_gettime(CLOCK_REALTIME, ts);
    ts->tv_sec += (time_t)(timeout / 1000U);
    ts->tv_nsec += (long)((timeout % 1000U) * 1000000UL);
    if(ts->tv_nsec >= 1000000000L) {
        ts->tv_sec += 1;
        ts->tv_nsec -= 1000000000L;
    }
}

FuriSemaphore* furi_semaphore_alloc(uint32_t max_count, uint32_t initial_count) {
    FuriSemaphore* semaphore = calloc(1, sizeof(FuriSemaphore));
    furi_check(semaphore != NULL);
    furi_check(max_count > 0U);
    furi_check(initial_count <= max_count);

    semaphore->max = max_count;
    semaphore->count = initial_count;

    pthread_mutex_init(&semaphore->lock, NULL);
    pthread_cond_init(&semaphore->cond, NULL);
    return semaphore;
}

void furi_semaphore_free(FuriSemaphore* instance) {
    furi_assert(instance);

    pthread_cond_destroy(&instance->cond);
    pthread_mutex_destroy(&instance->lock);
    free(instance);
}

FuriStatus furi_semaphore_acquire(FuriSemaphore* instance, uint32_t timeout) {
    furi_assert(instance);

    pthread_mutex_lock(&instance->lock);

    if(timeout == FuriWaitForever) {
        while(instance->count == 0U) {
            pthread_cond_wait(&instance->cond, &instance->lock);
        }
    } else if(timeout == 0U) {
        if(instance->count == 0U) {
            pthread_mutex_unlock(&instance->lock);
            return FuriStatusErrorTimeout;
        }
    } else {
        struct timespec abs_timeout;
        furi_semaphore_timeout_to_abs(timeout, &abs_timeout);
        while(instance->count == 0U) {
            const int result = pthread_cond_timedwait(&instance->cond, &instance->lock, &abs_timeout);
            if(result == ETIMEDOUT) {
                pthread_mutex_unlock(&instance->lock);
                return FuriStatusErrorTimeout;
            }
        }
    }

    instance->count--;

    pthread_mutex_unlock(&instance->lock);
    return FuriStatusOk;
}

FuriStatus furi_semaphore_release(FuriSemaphore* instance) {
    furi_assert(instance);

    pthread_mutex_lock(&instance->lock);

    if(instance->count == instance->max) {
        pthread_mutex_unlock(&instance->lock);
        return FuriStatusErrorResource;
    }

    instance->count++;
    pthread_cond_signal(&instance->cond);
    pthread_mutex_unlock(&instance->lock);
    return FuriStatusOk;
}

uint32_t furi_semaphore_get_count(FuriSemaphore* instance) {
    furi_assert(instance);

    pthread_mutex_lock(&instance->lock);
    const uint32_t count = instance->count;
    pthread_mutex_unlock(&instance->lock);
    return count;
}