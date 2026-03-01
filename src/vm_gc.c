#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vm.h"

void vm_free_map(Vm *vm, ObjMap *map) {
    for (size_t i = 0; i < map->capacity; i++) {
        ObjMapEntry entry = map->entries[i];

        if (entry.key != NULL) {
            free(entry.key->items);

            vm->bytes_allocated -= entry.key->count;

            vm_free_value(vm, entry.value);
        }
    }

    free(map->entries);

    vm->bytes_allocated -= map->capacity * sizeof(ObjMapEntry) + sizeof(ObjMap);
}

void vm_mark_object(Vm *vm, Obj *obj) {
    if (obj->marked)
        return;

    obj->marked = true;

    switch (obj->tag) {
    case OBJ_ARRAY: {
        ObjArray *arr = (ObjArray *)obj;

        for (size_t i = 0; i < arr->count; i++) {
            vm_mark_value(vm, arr->items[i]);
        }

        break;
    }

    case OBJ_MAP: {
        ObjMap *map = (ObjMap *)obj;

        for (size_t i = 0; i < map->capacity; i++) {
            ObjMapEntry entry = map->entries[i];

            if (entry.key != NULL) {
                entry.key->obj.marked = true;

                vm_mark_value(vm, entry.value);
            }
        }

        break;
    }

    case OBJ_FUNCTION: {
        ObjFunction *fn = (ObjFunction *)obj;

        for (size_t j = 0; j < fn->chunk.constants.count; j++) {
            vm_mark_value(vm, fn->chunk.constants.items[j]);
        }

        if (fn->globals != NULL) {
            vm_mark_object(vm, &fn->globals->obj);
        }

        break;
    }

    case OBJ_CLOSURE: {
        ObjClosure *closure = (ObjClosure *)obj;

        vm_mark_object(vm, &closure->fn->obj);

        for (size_t j = 0; j < closure->upvalues_count; j++) {
            vm_mark_object(vm, &closure->upvalues[j]->obj);
        }

        break;
    }

    case OBJ_UPVALUE: {
        ObjUpvalue *upvalue = (ObjUpvalue *)obj;

        vm_mark_value(vm, *upvalue->location);
        vm_mark_value(vm, upvalue->closed);

        if (upvalue->next != NULL) {
            vm_mark_object(vm, &upvalue->next->obj);
        }

        break;
    }

    case OBJ_STRING:
    case OBJ_NATIVE:
        break;
    }
}

void vm_mark_value(Vm *vm, Value value) {
    if (IS_OBJ(value)) {
        vm_mark_object(vm, AS_OBJ(value));
    }
}

void vm_mark_roots(Vm *vm) {
    for (Value *v = vm->stack; v < vm->sp; v++) {
        vm_mark_value(vm, *v);
    }

    for (size_t i = 0; i < vm->frame_count; i++) {
        CallFrame frame = vm->frames[i];

        vm_mark_object(vm, &frame.closure->fn->obj);
    }
}

void vm_free_object(Vm *vm, Obj *obj) {
    switch (obj->tag) {
    case OBJ_STRING: {
        ObjString *str = (ObjString *)obj;

        free(str->items);

        vm->bytes_allocated -= str->count * sizeof(char) + sizeof(ObjString);

        break;
    }

    case OBJ_ARRAY: {
        ObjArray *arr = (ObjArray *)obj;

        for (size_t i = 0; i < arr->count; i++) {
            vm_free_value(vm, arr->items[i]);
        }

        free(arr->items);

        vm->bytes_allocated -= arr->capacity * sizeof(Value) + sizeof(ObjArray);

        break;
    }

    case OBJ_MAP: {
        vm_free_map(vm, (ObjMap *)obj);

        break;
    }

    case OBJ_FUNCTION: {
        ObjFunction *fn = (ObjFunction *)obj;

        for (size_t i = 0; i < fn->chunk.constants.count; i++) {
            vm_free_value(vm, fn->chunk.constants.items[i]);
        }

        free(fn->chunk.constants.items);

        free(fn->chunk.bytes);
        free(fn->chunk.sources);

        vm->bytes_allocated -=
            fn->chunk.constants.capacity * sizeof(Value) + sizeof(ObjFunction);

        break;
    }

    case OBJ_CLOSURE: {
        ObjClosure *closure = (ObjClosure *)obj;

        for (uint8_t i = 0; i < closure->upvalues_count; i++) {
            vm_free_object(vm, &closure->upvalues[i]->obj);
        }

        free(closure->upvalues);

        vm_free_object(vm, &closure->fn->obj);

        vm->bytes_allocated -=
            closure->upvalues_count * sizeof(ObjUpvalue *) + sizeof(ObjClosure);

        break;
    }

    case OBJ_UPVALUE: {
        vm->bytes_allocated -= sizeof(ObjUpvalue);

        break;
    }

    case OBJ_NATIVE:
        vm->bytes_allocated -= sizeof(ObjNative);

        break;
    }

    free(obj);
}

void vm_free_value(Vm *vm, Value value) {
    if (IS_OBJ(value)) {
        vm_free_object(vm, AS_OBJ(value));
    }
}

static void vm_delete_white_strings(Vm *vm) {
    for (uint32_t i = 0; i < vm->strings->capacity; i++) {
        ObjMapEntry *entry = &vm->strings->entries[i];

        if (entry->key != NULL && !entry->key->obj.marked) {
            vm_map_delete(vm->strings, entry->key);
        }
    }
}

static void vm_sweep_objects(Vm *vm) {
    Obj *prev = NULL;
    Obj *curr = vm->objects;

    while (curr != NULL) {
        if (curr->marked) {
            curr->marked = false;
            prev = curr;
            curr = curr->next;
        } else {
            Obj *unreached = curr;

            if (prev == NULL) {
                vm->objects = curr->next;
            } else {
                prev->next = curr->next;
            }

            curr = curr->next;

            vm_free_object(vm, unreached);
        }
    }
}

void vm_gc(Vm *vm) {
    vm_mark_roots(vm);
    vm_delete_white_strings(vm);
    vm_sweep_objects(vm);

    vm->next_gc = vm->bytes_allocated * VM_GC_GROW_FACTOR;
}

Obj *vm_alloc(Vm *vm, ObjTag tag, size_t size) {
    vm->bytes_allocated += size;

    if (vm->bytes_allocated > vm->next_gc) {
        vm_gc(vm);
    }

    Obj *object = malloc(size);

    if (object == NULL) {
        fprintf(stderr, "error: out of memory\n");

        exit(1);
    }

    object->tag = tag;
    object->marked = false;
    object->next = vm->objects;

    vm->objects = object;

    return object;
}

ObjFunction *vm_new_function(Vm *vm, ObjMap *globals, Chunk chunk,
                             uint8_t arity, uint8_t upvalues_count) {
    ObjFunction *function = OBJ_ALLOC(vm, OBJ_FUNCTION, ObjFunction);

    function->globals = globals;
    function->chunk = chunk;
    function->arity = arity;
    function->upvalues_count = upvalues_count;

    return function;
}

ObjClosure *vm_new_closure(Vm *vm, ObjFunction *fn) {
    vm->bytes_allocated += fn->upvalues_count * sizeof(ObjUpvalue);

    if (vm->bytes_allocated > vm->next_gc) {
        vm_gc(vm);
    }

    ObjClosure *closure = OBJ_ALLOC(vm, OBJ_CLOSURE, ObjClosure);

    ObjUpvalue **upvalues = malloc(fn->upvalues_count * sizeof(ObjUpvalue));

    for (uint8_t i = 0; i < fn->upvalues_count; i++) {
        upvalues[i] = NULL;
    }

    closure->fn = fn;
    closure->upvalues = upvalues;
    closure->upvalues_count = fn->upvalues_count;

    return closure;
}

ObjUpvalue *vm_new_upvalue(Vm *vm, Value *location) {
    ObjUpvalue *upvalue = OBJ_ALLOC(vm, OBJ_UPVALUE, ObjUpvalue);

    upvalue->location = location;
    upvalue->closed = NULL_VAL;
    upvalue->next = NULL;

    return upvalue;
}

ObjMap *vm_new_map(Vm *vm) {
    ObjMap *map = OBJ_ALLOC(vm, OBJ_MAP, ObjMap);

    map->entries = NULL;
    map->count = 0;
    map->capacity = 0;

    return map;
}

ObjString *vm_new_string(Vm *vm, char *items, uint32_t count, uint32_t hash) {
    ObjString *string = OBJ_ALLOC(vm, OBJ_STRING, ObjString);

    string->items = items;
    string->count = count;
    string->hash = hash;

    vm_map_insert(vm, vm->strings, string, NULL_VAL);

    return string;
}

ObjString *vm_copy_string(Vm *vm, const char *items, uint32_t count) {
    vm->bytes_allocated += count * sizeof(*items);

    if (vm->bytes_allocated > vm->next_gc) {
        vm_gc(vm);
    }

    uint32_t hash = string_hash(items, count);

    ObjString *interned = vm_find_string(vm, items, count, hash);

    if (interned != NULL) {
        return interned;
    }

    char *copied_items = malloc(count * sizeof(*items));

    if (copied_items == NULL) {
        fprintf(stderr, "error: out of memory\n");

        exit(1);
    }

    memcpy(copied_items, items, count * sizeof(*items));

    return vm_new_string(vm, copied_items, count, hash);
}

ObjString *vm_concat_strings(Vm *vm, ObjString *lhs, ObjString *rhs) {
    vm->bytes_allocated += lhs->count + rhs->count;

    if (vm->bytes_allocated > vm->next_gc) {
        vm_gc(vm);
    }

    char *items = malloc((lhs->count + rhs->count) * sizeof(char));

    if (items == NULL) {
        fprintf(stderr, "error: out of memory\n");

        exit(1);
    }

    memcpy(items, lhs->items, lhs->count);
    memcpy(items + lhs->count, rhs->items, rhs->count);

    uint32_t count = lhs->count + rhs->count;

    uint32_t hash = string_hash(items, count);

    ObjString *interned = vm_find_string(vm, items, count, hash);

    if (interned != NULL) {
        free(items);

        return interned;
    }

    return vm_new_string(vm, items, count, hash);
}

ObjArray *vm_copy_array(Vm *vm, const Value *items, uint32_t count) {
    vm->bytes_allocated += count * sizeof(*items);

    if (vm->bytes_allocated > vm->next_gc) {
        vm_gc(vm);
    }

    ObjArray *array = OBJ_ALLOC(vm, OBJ_ARRAY, ObjArray);

    array->items = malloc(count * sizeof(*items));

    if (array->items == NULL) {
        fprintf(stderr, "error: out of memory\n");

        exit(1);
    }

    memcpy(array->items, items, count * sizeof(*items));

    array->count = count;
    array->capacity = count;

    return array;
}

ObjArray *vm_concat_arrays(Vm *vm, ObjArray *lhs, ObjArray *rhs) {
    vm->bytes_allocated += (lhs->count + rhs->count) * sizeof(Value);

    if (vm->bytes_allocated > vm->next_gc) {
        vm_gc(vm);
    }

    ObjArray *array = OBJ_ALLOC(vm, OBJ_ARRAY, ObjArray);

    array->items = malloc((lhs->count + rhs->count) * sizeof(Value));

    if (array->items == NULL) {
        fprintf(stderr, "error: out of memory\n");

        exit(1);
    }

    memcpy(array->items, lhs->items, lhs->count * sizeof(Value));
    memcpy(array->items + lhs->count, rhs->items, rhs->count * sizeof(Value));

    array->count = lhs->count + rhs->count;

    array->capacity = array->count;

    return array;
}
