#include "event_flag.h"
#include "check.h"

#include <errno.h>
#include <time.h>

#define FURI_EVENT_FLAG_MAX_BITS_EVENT_GROUPS 24U
#define FURI_EVENT_FLAG_INVALID_BITS (~((1UL << FURI_EVENT_FLAG_MAX_BITS_EVENT_GROUPS) - 1U))

FuriEventFlag* furi_event_flag_alloc() {
    FuriEventFlag* instance = calloc(1, sizeof(FuriEventFlag));
    furi_check(instance != NULL);
    pthread_mutex_init(&instance->lock, NULL);
    pthread_cond_init(&instance->cond, NULL);
    return instance;
}

void furi_event_flag_free(FuriEventFlag* instance) {
    furi_assert(instance);
    pthread_cond_destroy(&instance->cond);
    pthread_mutex_destroy(&instance->lock);
    free(instance);
}

uint32_t furi_event_flag_set(FuriEventFlag* instance, uint32_t flags) {
    furi_assert(instance);

    if((flags & FURI_EVENT_FLAG_INVALID_BITS) != 0U) {
        return FuriFlagErrorParameter;
    }

    pthread_mutex_lock(&instance->lock);
    instance->flags |= flags;
    const uint32_t result = instance->flags;
    pthread_cond_broadcast(&instance->cond);
    pthread_mutex_unlock(&instance->lock);
    return result;
}

uint32_t furi_event_flag_clear(FuriEventFlag* instance, uint32_t flags) {
    furi_assert(instance);

    if((flags & FURI_EVENT_FLAG_INVALID_BITS) != 0U) {
        return FuriFlagErrorParameter;
    }

    pthread_mutex_lock(&instance->lock);
    instance->flags &= ~flags;
    const uint32_t result = instance->flags;
    pthread_mutex_unlock(&instance->lock);
    return result;
}

uint32_t furi_event_flag_get(FuriEventFlag* instance) {
    furi_assert(instance);

    pthread_mutex_lock(&instance->lock);
    const uint32_t flags = instance->flags;
    pthread_mutex_unlock(&instance->lock);
    return flags;
}

static bool furi_event_flag_is_satisfied(uint32_t current, uint32_t flags, uint32_t options) {
    if((options & FuriFlagWaitAll) == FuriFlagWaitAll) {
        return (current & flags) == flags;
    }
    return (current & flags) != 0U;
}

static void furi_event_flag_timeout_to_abs(uint32_t timeout, struct timespec* ts) {
    clock_gettime(CLOCK_REALTIME, ts);
    ts->tv_sec += (time_t)(timeout / 1000U);
    ts->tv_nsec += (long)((timeout % 1000U) * 1000000UL);
    if(ts->tv_nsec >= 1000000000L) {
        ts->tv_sec += 1;
        ts->tv_nsec -= 1000000000L;
    }
}

uint32_t furi_event_flag_wait(
    FuriEventFlag* instance,
    uint32_t flags,
    uint32_t options,
    uint32_t timeout) {
    furi_assert(instance);

    if(flags == 0U || (flags & FURI_EVENT_FLAG_INVALID_BITS) != 0U) {
        return FuriFlagErrorParameter;
    }

    pthread_mutex_lock(&instance->lock);

    if(timeout == 0U) {
        if(!furi_event_flag_is_satisfied(instance->flags, flags, options)) {
            pthread_mutex_unlock(&instance->lock);
            return FuriFlagErrorTimeout;
        }
    } else if(timeout == FuriWaitForever) {
        while(!furi_event_flag_is_satisfied(instance->flags, flags, options)) {
            pthread_cond_wait(&instance->cond, &instance->lock);
        }
    } else {
        struct timespec abs_timeout;
        furi_event_flag_timeout_to_abs(timeout, &abs_timeout);
        while(!furi_event_flag_is_satisfied(instance->flags, flags, options)) {
            const int result = pthread_cond_timedwait(&instance->cond, &instance->lock, &abs_timeout);
            if(result == ETIMEDOUT) {
                pthread_mutex_unlock(&instance->lock);
                return FuriFlagErrorTimeout;
            }
        }
    }

    const uint32_t matched_flags = instance->flags & flags;
    if((options & FuriFlagNoClear) == 0U) {
        instance->flags &= ~flags;
    }

    pthread_mutex_unlock(&instance->lock);
    return matched_flags;
}
