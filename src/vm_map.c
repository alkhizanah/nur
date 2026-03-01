#include <stdlib.h>
#include <string.h>

#include "vm.h"

static ObjMapEntry *vm_map_find_entry(ObjMapEntry *entries, uint32_t capacity,
                                      ObjString *key) {
    uint32_t index = key->hash & (capacity - 1);

    ObjMapEntry *tombstone = NULL;

    for (;;) {
        ObjMapEntry *entry = &entries[index];

        if (entry->key == NULL) {
            if (IS_NULL(entry->value)) {
                return tombstone != NULL ? tombstone : entry;
            } else if (tombstone == NULL) {
                tombstone = entry;
            }
        } else if (entry->key == key) { // The benefit of string interning
            return entry;
        }

        index = (index + 1) & (capacity - 1);
    }
}

ObjString *vm_find_string(Vm *vm, const char *key, uint32_t count,
                          uint32_t hash) {
    if (vm->strings->count == 0) {
        return NULL;
    }

    uint32_t index = hash & (vm->strings->capacity - 1);

    for (;;) {
        ObjMapEntry *entry = &vm->strings->entries[index];

        if (entry->key == NULL && IS_NULL(entry->value)) {
            return NULL;
        } else if (entry->key->count == count && entry->key->hash == hash &&
                   memcmp(entry->key->items, key, count) == 0) {
            return entry->key;
        }

        index = (index + 1) & (vm->strings->capacity - 1);
    }
}

bool vm_map_lookup(const ObjMap *map, ObjString *key, Value *value) {
    if (map->count == 0) {
        return false;
    }

    ObjMapEntry *entry = vm_map_find_entry(map->entries, map->capacity, key);

    if (entry->key == NULL) {
        return false;
    }

    *value = entry->value;

    return true;
}

bool vm_map_remove(ObjMap *map, ObjString *key) {
    if (map->count == 0) {
        return false;
    }

    ObjMapEntry *entry = vm_map_find_entry(map->entries, map->capacity, key);

    if (entry->key == NULL) {
        return false;
    }

    entry->key = NULL;
    entry->value = BOOL_VAL(true);

    return true;
}

void vm_map_adjust_capacity(Vm *vm, ObjMap *map, uint32_t capacity) {
    vm->bytes_allocated += capacity * sizeof(ObjMapEntry);

    if (vm->bytes_allocated > vm->next_gc) {
        vm_gc(vm);
    }

    ObjMapEntry *entries = malloc(capacity * sizeof(ObjMapEntry));

    for (uint32_t i = 0; i < capacity; i++) {
        entries[i].key = NULL;
        entries[i].value = NULL_VAL;
    }

    map->count = 0;

    for (uint32_t i = 0; i < map->capacity; i++) {
        ObjMapEntry *entry = &map->entries[i];

        if (entry->key == NULL)
            continue;

        ObjMapEntry *dest = vm_map_find_entry(entries, capacity, entry->key);

        dest->key = entry->key;
        dest->value = entry->value;

        map->count++;
    }

    free(map->entries);

    vm->bytes_allocated -= map->capacity * sizeof(ObjMapEntry);

    map->entries = entries;
    map->capacity = capacity;
}

#define VM_MAP_MAX_LOAD 0.75

bool vm_map_insert(Vm *vm, ObjMap *map, ObjString *key, Value value) {

    if (map->count + 1 > map->capacity * VM_MAP_MAX_LOAD) {
        vm_map_adjust_capacity(vm, map,
                               map->capacity == 0 ? 8 : map->capacity * 2);
    }

    ObjMapEntry *entry = vm_map_find_entry(map->entries, map->capacity, key);

    bool is_new_key = entry->key == NULL;

    if (is_new_key && IS_NULL(entry->value)) {
        map->count++;
    }

    entry->key = key;
    entry->value = value;

    return is_new_key;
}

bool vm_map_delete(ObjMap *map, ObjString *key) {
    if (map->count == 0)
        return false;

    ObjMapEntry *entry = vm_map_find_entry(map->entries, map->capacity, key);

    if (entry->key == NULL)
        return false;

    entry->key = NULL;
    entry->value = BOOL_VAL(true);

    return true;
}
