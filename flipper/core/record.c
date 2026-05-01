#include "record.h"
#include <ptr_dict.h>
#include <pthread.h>

PtrDict* records;
bool initialized = false;
static pthread_mutex_t records_mutex = PTHREAD_MUTEX_INITIALIZER;

void furi_record_init() {
    pthread_mutex_lock(&records_mutex);
    if(initialized) {
        pthread_mutex_unlock(&records_mutex);
        return;
    }

    records = ptr_dict_alloc();
    initialized = true;
    pthread_mutex_unlock(&records_mutex);
}
bool furi_record_status() {
    pthread_mutex_lock(&records_mutex);
    const bool status = initialized;
    pthread_mutex_unlock(&records_mutex);
    return status;
}

void furi_record_create(const char* name, void* data) {
    pthread_mutex_lock(&records_mutex);
    ptr_dict_set(records, name, data);
    pthread_mutex_unlock(&records_mutex);
}

void* furi_record_open(const char* name) {
    pthread_mutex_lock(&records_mutex);
    void* value = ptr_dict_get(records, name);
    pthread_mutex_unlock(&records_mutex);
    return value;
}

void furi_record_close(const char* name) {
    (void)name;
    // TODO
}
