#include "mutex.h"
#include "check.h"
#include <errno.h>
#include <time.h>

static void furi_mutex_timeout_to_abs(uint32_t timeout, struct timespec* ts) {
    clock_gettime(CLOCK_REALTIME, ts);
    ts->tv_sec += (time_t)(timeout / 1000U);
    ts->tv_nsec += (long)((timeout % 1000U) * 1000000UL);
    if(ts->tv_nsec >= 1000000000L) {
        ts->tv_sec += 1;
        ts->tv_nsec -= 1000000000L;
    }
}

FuriMutex* furi_mutex_alloc(FuriMutexType type) {
    FuriMutex* mutex = calloc(1, sizeof(FuriMutex));
    furi_check(mutex != NULL);
    mutex->type = type;
    mutex->owner = MUTEX_NO_OWNER;
    pthread_mutex_init(&mutex->lock, NULL);
    pthread_cond_init(&mutex->cond, NULL);
    return mutex;
}

void furi_mutex_free(FuriMutex* instance) {
    furi_assert(instance);

    pthread_cond_destroy(&instance->cond);
    pthread_mutex_destroy(&instance->lock);
    free(instance);
}

FuriStatus furi_mutex_acquire(FuriMutex* instance, uint32_t timeout) {
    furi_assert(instance);
    const FuriThreadId self = furi_thread_get_current_id();

    pthread_mutex_lock(&instance->lock);

    if(instance->owner == self) {
        if(instance->type != FuriMutexTypeRecursive) {
            pthread_mutex_unlock(&instance->lock);
            return FuriStatusErrorResource;
        }
        instance->lock_count++;
        pthread_mutex_unlock(&instance->lock);
        return FuriStatusOk;
    }

    if(timeout == FuriWaitForever) {
        while(instance->owner != MUTEX_NO_OWNER) {
            pthread_cond_wait(&instance->cond, &instance->lock);
        }
    } else if(timeout == 0U) {
        if(instance->owner != MUTEX_NO_OWNER) {
            pthread_mutex_unlock(&instance->lock);
            return FuriStatusErrorTimeout;
        }
    } else {
        struct timespec abs_timeout;
        furi_mutex_timeout_to_abs(timeout, &abs_timeout);
        while(instance->owner != MUTEX_NO_OWNER) {
            const int result = pthread_cond_timedwait(&instance->cond, &instance->lock, &abs_timeout);
            if(result == ETIMEDOUT) {
                pthread_mutex_unlock(&instance->lock);
                return FuriStatusErrorTimeout;
            }
        }
    }

    instance->owner = self;
    instance->lock_count = 1U;
    pthread_mutex_unlock(&instance->lock);
    return FuriStatusOk;
}

FuriStatus furi_mutex_release(FuriMutex* instance) {
    furi_assert(instance);
    const FuriThreadId self = furi_thread_get_current_id();

    pthread_mutex_lock(&instance->lock);
    if(instance->owner != self || instance->lock_count == 0U) {
        pthread_mutex_unlock(&instance->lock);
        return FuriStatusErrorResource;
    }

    instance->lock_count--;
    if(instance->lock_count == 0U) {
        instance->owner = MUTEX_NO_OWNER;
        pthread_cond_signal(&instance->cond);
    }
    pthread_mutex_unlock(&instance->lock);
    return FuriStatusOk;
}

FuriThreadId furi_mutex_get_owner(FuriMutex* instance) {
    furi_assert(instance);
    pthread_mutex_lock(&instance->lock);
    const FuriThreadId owner = instance->owner;
    pthread_mutex_unlock(&instance->lock);
    return owner;
}