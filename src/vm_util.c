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

    default:
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

    default:
        return false;
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

    default:
        return false;
    }
}

int64_t value_to_int(Value value) {
    if (IS_INT(value)) {
        return AS_INT(value);
    } else if (IS_FLT(value)) {
        return AS_FLT(value);
    } else if (IS_BOOL(value)) {
        return AS_BOOL(value);
    } else {
        return INT64_MAX;
    }
}

double value_to_float(Value value) {
    if (IS_INT(value)) {
        return AS_INT(value);
    } else if (IS_FLT(value)) {
        return AS_FLT(value);
    } else if (IS_BOOL(value)) {
        return AS_BOOL(value);
    } else {
        return 0.0 / 0.0;
    }
}

bool value_is_falsey(Value v) {
    return IS_NULL(v) || (IS_BOOL(v) && !AS_BOOL(v)) ||
           (IS_INT(v) && AS_INT(v) == 0) || (IS_FLT(v) && AS_FLT(v) == 0) ||
           (IS_STRING(v) && AS_STRING(v)->count == 0) ||
           (IS_ARRAY(v) && AS_ARRAY(v)->count == 0) ||
           (IS_MAP(v) && AS_MAP(v)->count == 0);
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
        default:
            return NULL;
        }
    default:
        return NULL;
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

uint32_t string_utf8_characters_count(const char *start, const char *end) {
    uint32_t count = 0;

    while (start < end) {
        start = string_utf8_skip_character(start);
        count++;
    }

    return count;
}

const char *string_utf8_skip_character(const char *start) {
    if ((*start & 0b10000000) == 0b00000000) {
        return start + 1;
    } else if ((*start & 0b11100000) == 0b11000000) {
        return start + 2;
    } else if ((*start & 0b11110000) == 0b11100000) {
        return start + 3;
    } else if ((*start & 0b11111000) == 0b11110000) {
        return start + 4;
    } else {
        return start;
    }
}

uint32_t string_utf8_encode_character(uint32_t rune, char *result) {
    if (rune <= 0x7F) {
        result[0] = rune;

        return 1;
    } else if (rune <= 0x7FF) {
        result[0] = 0xC0 | ((rune >> 6) & 0x1F);
        result[1] = 0x80 | (rune & 0x3F);

        return 2;
    } else if (rune <= 0xFFFF) {
        result[0] = 0xE0 | ((rune >> 12) & 0x0F);
        result[1] = 0x80 | ((rune >> 6) & 0x3F);
        result[2] = 0x80 | (rune & 0x3F);

        return 3;
    } else if (rune <= 0x10FFFF) {
        result[0] = 0xF0 | ((rune >> 18) & 0x07);
        result[1] = 0x80 | ((rune >> 12) & 0x3F);
        result[2] = 0x80 | ((rune >> 6) & 0x3F);
        result[3] = 0x80 | (rune & 0x3F);

        return 4;
    } else {
        return 0;
    }
}

const char *string_utf8_decode_character(const char *start, uint32_t *rune) {
    const char *next;

    if ((*start & 0b10000000) == 0b00000000) {
        *rune = start[0];

        next = start + 1;
    } else if ((*start & 0b11100000) == 0b11000000) {
        *rune = ((uint32_t)(start[0] & 0x1f) << 6) |
                ((uint32_t)(start[1] & 0x3f) << 0);

        next = start + 2;
    } else if ((*start & 0b11110000) == 0b11100000) {
        *rune = ((uint32_t)(start[0] & 0x0f) << 12) |
                ((uint32_t)(start[1] & 0x3f) << 6) |
                ((uint32_t)(start[2] & 0x3f) << 0);

        next = start + 3;
    } else if ((*start & 0b11111000) == 0b11110000) {
        *rune = ((uint32_t)(start[0] & 0x07) << 18) |
                ((uint32_t)(start[1] & 0x3f) << 12) |
                ((uint32_t)(start[2] & 0x3f) << 6) |
                ((uint32_t)(start[3] & 0x3f) << 0);

        next = start + 4;
    } else {
        *rune = 0;

        next = start + 1;
    }

    if (*rune >= 0xD800 && *rune <= 0xDFFF)
        *rune = 0;

    return next;
}
