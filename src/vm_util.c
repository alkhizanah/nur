#include <stdio.h>

#include "array.h"
#include "vm.h"

bool chunks_equal(Chunk a, Chunk b) {
    if (a.count != b.count || a.constants.count != b.constants.count) {
        return false;
    }

    for (size_t i = 0; i < a.count; i++) {
        if (a.bytes[i] != b.bytes[i] || a.sources[i] != b.sources[i]) {
            return false;
        }
    }

    for (size_t i = 0; i < a.constants.count; i++) {
        if (!values_equal(a.constants.items[i], b.constants.items[i])) {
            return false;
        }
    }

    return true;
}

bool objects_equal(Obj *a, Obj *b) {
    if (a == b) {
        return true;
    }

    if (a->tag != b->tag) {
        return false;
    }

    switch (a->tag) {
    case OBJ_STRING:
        if (((ObjString *)a)->count != ((ObjString *)b)->count) {
            return false;
        }

        if (((ObjString *)a)->items == ((ObjString *)b)->items) {
            return true;
        }

        for (uint32_t i = 0; i < ((ObjString *)a)->count; i++) {
            if (((ObjString *)a)->items[i] != ((ObjString *)b)->items[i]) {
                return false;
            }
        }

        return true;

    case OBJ_FUNCTION:
        if (((ObjFunction *)a)->arity != ((ObjFunction *)b)->arity) {
            return false;
        }

        return chunks_equal(((ObjFunction *)a)->chunk,
                            ((ObjFunction *)b)->chunk);

    case OBJ_CLOSURE:
        return objects_equal(&((ObjClosure *)a)->fn->obj,
                             &((ObjClosure *)b)->fn->obj);

    case OBJ_ARRAY:
        if (((ObjArray *)a)->count != ((ObjArray *)b)->count) {
            return false;
        }

        if (((ObjArray *)a)->items == ((ObjArray *)b)->items) {
            return true;
        }

        for (uint32_t i = 0; i < ((ObjArray *)a)->count; i++) {
            if (!values_equal(((ObjArray *)a)->items[i],
                              ((ObjArray *)b)->items[i])) {
                return false;
            }
        }

        return true;

    case OBJ_MAP:
        if (((ObjMap *)a)->count != ((ObjMap *)b)->count) {
            return false;
        }

        if (((ObjMap *)a)->entries == ((ObjMap *)b)->entries) {
            return true;
        }

        for (uint32_t i = 0; i < ((ObjMap *)a)->count; i++) {
            ObjMapEntry ae = ((ObjMap *)a)->entries[i];

            if (ae.key != NULL) {
                Value bv;

                if (!vm_map_lookup((ObjMap *)b, ae.key, &bv)) {
                    return false;
                }

                if (!values_equal(ae.value, bv)) {
                    return false;
                }
            }
        }

        return true;

    case OBJ_NATIVE:
        return ((ObjNative *)a)->fn == ((ObjNative *)b)->fn;

    case OBJ_UPVALUE:
        return false;
    }
}

bool values_exactly_equal(Value a, Value b) {
    if (a.tag != b.tag) {
        return false;
    }

    switch (a.tag) {
    case VAL_NULL:
        return true;

    case VAL_BOOL:
        return AS_BOOL(a) == AS_BOOL(b);

    case VAL_INT:
        return AS_INT(a) == AS_INT(b);

    case VAL_FLT:
        return AS_FLT(a) == AS_FLT(b);

    case VAL_OBJ:
        return objects_equal(AS_OBJ(a), AS_OBJ(b));
    }
}

bool values_equal(Value a, Value b) {
    if (a.tag != b.tag && !((a.tag == VAL_INT && b.tag == VAL_FLT) ||
                            (a.tag == VAL_FLT && b.tag == VAL_INT))) {
        return false;
    }

    switch (a.tag) {
    case VAL_NULL:
        return true;

    case VAL_BOOL:
        return AS_BOOL(a) == AS_BOOL(b);

    case VAL_INT:
        if (b.tag == VAL_FLT) {
            return AS_INT(a) == AS_FLT(b);
        } else {
            return AS_INT(a) == AS_INT(b);
        }

    case VAL_FLT:
        if (b.tag == VAL_INT) {
            return AS_FLT(a) == AS_INT(b);
        } else {
            return AS_FLT(a) == AS_FLT(b);
        }

    case VAL_OBJ:
        return objects_equal(AS_OBJ(a), AS_OBJ(b));
    }
}

bool value_is_falsey(Value value) {
    return IS_NULL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

const char *value_description(Value value) {
    switch (value.tag) {
    case VAL_NULL:
        return "a null";
    case VAL_BOOL:
        return "a boolean";
    case VAL_INT:
        return "an integer";
    case VAL_FLT:
        return "a floating point";
    case VAL_OBJ:
        switch (AS_OBJ(value)->tag) {
        case OBJ_STRING:
            return "a string";

        case OBJ_ARRAY:
            return "an array";

        case OBJ_MAP:
            return "a map";

        case OBJ_CLOSURE:
        case OBJ_FUNCTION:
        case OBJ_NATIVE:
            return "a function";

        case OBJ_UPVALUE:
            return "an upvalue";
        }
    }
}

void object_display(Obj *obj) {
    switch (obj->tag) {
    case OBJ_STRING: {
        ObjString *str = (ObjString *)obj;
        printf("\"%.*s\"", (int)str->count, str->items);
        break;
    }

    case OBJ_ARRAY: {
        ObjArray *arr = (ObjArray *)obj;

        printf("[");

        if (arr->count > 0) {
            if (IS_STRING(arr->items[0])) {
                printf("\"");
            }

            value_display(arr->items[0]);

            if (IS_STRING(arr->items[0])) {
                printf("\"");
            }

            for (size_t i = 1; i < arr->count; i++) {
                printf(", ");

                if (IS_STRING(arr->items[i])) {
                    printf("\"");
                }

                value_display(arr->items[i]);

                if (IS_STRING(arr->items[i])) {
                    printf("\"");
                }
            }
        }

        printf("]");

        break;
    }

    case OBJ_MAP: {
        ObjMap *map = (ObjMap *)obj;

        printf("{");

        bool first = true;

        for (uint32_t i = 0; i < map->capacity; i++) {
            ObjMapEntry entry = map->entries[i];

            if (entry.key != NULL) {
                if (first) {
                    first = false;
                } else {
                    printf(", ");
                }

                printf("\"%.*s\": ", (int)entry.key->count, entry.key->items);
                value_display(entry.value);
            }
        }

        printf("}");

        break;
    }

    case OBJ_CLOSURE:
    case OBJ_FUNCTION:
    case OBJ_NATIVE:
        printf("<function>");

        break;

    case OBJ_UPVALUE:
        printf("<upvalue>");

        break;
    }
}

void value_display(Value value) {
    switch (value.tag) {
    case VAL_NULL:
        printf("null");
        break;

    case VAL_BOOL:
        printf(AS_BOOL(value) ? "true" : "false");
        break;

    case VAL_INT:
        printf("%ld", AS_INT(value));
        break;

    case VAL_FLT:
        printf("%g", AS_FLT(value));
        break;

    case VAL_OBJ:
        object_display(AS_OBJ(value));
        break;
    }
}

size_t chunk_add_constant(Chunk *chunk, Value value) {
    for (size_t i = 0; i < chunk->constants.count; i++) {
        if (values_exactly_equal(chunk->constants.items[i], value)) {
            return i;
        }
    }

    size_t i = chunk->constants.count;

    ARRAY_PUSH(&chunk->constants, value);

    return i;
}

size_t chunk_add_byte(Chunk *chunk, uint8_t byte, uint32_t source) {
    if (chunk->count + 1 > chunk->capacity) {
        size_t new_cap =
            chunk->capacity ? chunk->capacity * 2 : ARRAY_INIT_CAPACITY;

        chunk->bytes = realloc(chunk->bytes, sizeof(*chunk->bytes) * new_cap);

        chunk->sources =
            realloc(chunk->sources, sizeof(*chunk->sources) * new_cap);

        if (chunk->bytes == NULL || chunk->sources == NULL) {
            fprintf(stderr, "error: out of memory\n");

            exit(1);
        }

        chunk->capacity = new_cap;
    }

    chunk->bytes[chunk->count] = byte;
    chunk->sources[chunk->count] = source;

    return chunk->count++;
}

void vm_push(Vm *vm, Value value) {
    *vm->sp = value;
    vm->sp++;
}

Value vm_pop(Vm *vm) {
    vm->sp--;
    return *vm->sp;
}

Value vm_peek(const Vm *vm, size_t distance) { return vm->sp[-1 - distance]; }

void vm_poke(Vm *vm, size_t distance, Value value) {
    vm->sp[-1 - distance] = value;
}

uint32_t string_hash(const char *key, uint32_t count) {
    uint32_t hash = 2166136261u;

    for (uint32_t i = 0; i < count; i++) {
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }

    return hash;
}
