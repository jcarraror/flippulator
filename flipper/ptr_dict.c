#include "ptr_dict.h"

PtrDictEl* ptr_dict_el_alloc() {
    return (PtrDictEl*) malloc(sizeof(PtrDictEl));
}
PtrDict* ptr_dict_alloc() {
    PtrDict* dict = malloc(sizeof(PtrDict));
    dict->first = NULL;
    return dict;
}

void ptr_dict_set(PtrDict* dict, const char* name, void* value) {
    PtrDictEl* current = dict->first;
    PtrDictEl* last = NULL;

    while(current != NULL) {
        if(strcmp(current->name, name) == 0) {
            current->value = value;
            return;
        }

        last = current;
        current = current->next;
    }

    PtrDictEl* entry = ptr_dict_el_alloc();
    entry->name = strdup(name);
    entry->value = value;
    entry->next = NULL;

    if(last == NULL) {
        dict->first = entry;
    } else {
        last->next = entry;
    }
}

void* ptr_dict_get(PtrDict* dict, const char* name) {
    PtrDictEl* el = dict->first;
    if(el == NULL) return NULL;
    while(strcmp(el->name, name) != 0 && el->next != NULL)
        el = el->next;
    if(strcmp(el->name, name) != 0)
        return NULL;
    return el->value;
}
