#include "message_queue.h"
#include "core_defines.h"
#include "kernel.h"
#include "check.h"
#include <pthread.h>
#include <errno.h>
#include <time.h>

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t can_read;
    pthread_cond_t can_write;
    uint32_t count;
    uint32_t capacity;
    uint32_t msg_size;
    uint32_t head;
    uint32_t tail;
    uint8_t buffer[];
} FuriMessageQueueImpl;

static FuriMessageQueueImpl* furi_message_queue_impl(FuriMessageQueue* queue) {
    return (FuriMessageQueueImpl*)queue;
}

static const FuriMessageQueueImpl* furi_message_queue_impl_const(const FuriMessageQueue* queue) {
    return (const FuriMessageQueueImpl*)queue;
}

static void furi_message_queue_timeout_to_abs(uint32_t timeout, struct timespec* ts) {
    clock_gettime(CLOCK_REALTIME, ts);
    ts->tv_sec += (time_t)(timeout / 1000U);
    ts->tv_nsec += (long)((timeout % 1000U) * 1000000UL);
    if(ts->tv_nsec >= 1000000000L) {
        ts->tv_sec += 1;
        ts->tv_nsec -= 1000000000L;
    }
}

FuriMessageQueue* furi_message_queue_alloc(uint32_t msg_count, uint32_t msg_size) {
    FuriMessageQueueImpl* queue = calloc(1, sizeof(FuriMessageQueueImpl) + msg_count * msg_size);
    furi_check(queue != NULL);

    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->can_read, NULL);
    pthread_cond_init(&queue->can_write, NULL);
    queue->capacity = msg_count;
    queue->msg_size = msg_size;
    return (FuriMessageQueue*)queue;
}

void furi_message_queue_free(FuriMessageQueue* queue) {
    furi_assert(queue);
    FuriMessageQueueImpl* impl = furi_message_queue_impl(queue);
    pthread_cond_destroy(&impl->can_write);
    pthread_cond_destroy(&impl->can_read);
    pthread_mutex_destroy(&impl->mutex);
    free(impl);
}
 
FuriStatus furi_message_queue_put(FuriMessageQueue* queue, const void* msg_ptr, uint32_t timeout) {
    furi_assert(queue);
    furi_assert(msg_ptr);

    FuriMessageQueueImpl* impl = furi_message_queue_impl(queue);
    pthread_mutex_lock(&impl->mutex);

    while(impl->count == impl->capacity) {
        if(timeout == 0U) {
            pthread_mutex_unlock(&impl->mutex);
            return FuriStatusErrorTimeout;
        } else if(timeout == FuriWaitForever) {
            pthread_cond_wait(&impl->can_write, &impl->mutex);
        } else {
            struct timespec ts;
            furi_message_queue_timeout_to_abs(timeout, &ts);
            const int result = pthread_cond_timedwait(&impl->can_write, &impl->mutex, &ts);
            if(result == ETIMEDOUT) {
                pthread_mutex_unlock(&impl->mutex);
                return FuriStatusErrorTimeout;
            }
        }
    }

    memcpy(impl->buffer + impl->tail * impl->msg_size, msg_ptr, impl->msg_size);
    impl->tail = (impl->tail + 1U) % impl->capacity;
    impl->count++;

    pthread_cond_signal(&impl->can_read);
    pthread_mutex_unlock(&impl->mutex);
    return FuriStatusOk;
}
 
FuriStatus furi_message_queue_get(FuriMessageQueue* queue, void* msg_ptr, uint32_t timeout) {
    furi_assert(queue);
    furi_assert(msg_ptr);

    FuriMessageQueueImpl* impl = furi_message_queue_impl(queue);
    pthread_mutex_lock(&impl->mutex);

    while(impl->count == 0U) {
        if(timeout == 0U) {
            pthread_mutex_unlock(&impl->mutex);
            return FuriStatusErrorTimeout;
        } else if(timeout == FuriWaitForever) {
            pthread_cond_wait(&impl->can_read, &impl->mutex);
        } else {
            struct timespec ts;
            furi_message_queue_timeout_to_abs(timeout, &ts);
            const int result = pthread_cond_timedwait(&impl->can_read, &impl->mutex, &ts);
            if(result == ETIMEDOUT) {
                pthread_mutex_unlock(&impl->mutex);
                return FuriStatusErrorTimeout;
            }
        }
    }

    memcpy(msg_ptr, impl->buffer + impl->head * impl->msg_size, impl->msg_size);
    impl->head = (impl->head + 1U) % impl->capacity;
    impl->count--;

    pthread_cond_signal(&impl->can_write);
    pthread_mutex_unlock(&impl->mutex);
    return FuriStatusOk;
}

uint32_t furi_message_queue_get_capacity(FuriMessageQueue* queue) {
    furi_assert(queue);
    const FuriMessageQueueImpl* impl = furi_message_queue_impl_const(queue);
    return impl->capacity;
}

uint32_t furi_message_queue_get_message_size(FuriMessageQueue* queue) {
    furi_assert(queue);
    const FuriMessageQueueImpl* impl = furi_message_queue_impl_const(queue);
    return impl->msg_size;
}

uint32_t furi_message_queue_get_count(FuriMessageQueue* queue) {
    furi_assert(queue);
    FuriMessageQueueImpl* impl = furi_message_queue_impl(queue);
    pthread_mutex_lock(&impl->mutex);
    const uint32_t count = impl->count;
    pthread_mutex_unlock(&impl->mutex);
    return count;
}

uint32_t furi_message_queue_get_space(FuriMessageQueue* queue) {
    furi_assert(queue);
    return furi_message_queue_get_capacity(queue) - furi_message_queue_get_count(queue);
}

FuriStatus furi_message_queue_reset(FuriMessageQueue* queue) {
    furi_assert(queue);
    FuriMessageQueueImpl* impl = furi_message_queue_impl(queue);
    pthread_mutex_lock(&impl->mutex);
    impl->count = 0U;
    impl->head = 0U;
    impl->tail = 0U;
    pthread_cond_broadcast(&impl->can_write);
    pthread_mutex_unlock(&impl->mutex);
    return FuriStatusOk;
}