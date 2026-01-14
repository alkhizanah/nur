#include <assert.h>

#include "array.h"
#include "vm.h"

void vm_init(Vm *vm) {
    vm_stack_reset(vm);

    vm->objects = NULL;
    vm->bytes_allocated = 0;
    vm->next_gc = 1024 * 1024;
}

void vm_stack_reset(Vm *vm) {
    vm->sp = vm->stack;
    vm->frame_count = 0;
}

static bool value_is_falsey(Value value) {
    return IS_NONE(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

bool vm_run(Vm *vm, Value *result) {
    CallFrame *frame = &vm->frames[vm->frame_count - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_SHORT() (((uint16_t)READ_BYTE() << 8) | READ_BYTE())
#define READ_CONSTANT() (frame->fn->chunk.constants.items[READ_SHORT()])
#define READ_STRING() (AS_STRING(READ_CONSTANT()))

    for (;;) {
        uint8_t opcode = READ_BYTE();

        switch (opcode) {
        case OP_NONE:
            vm_push(vm, NONE_VAL);
            break;

        case OP_TRUE:
            vm_push(vm, BOOL_VAL(true));
            break;

        case OP_FALSE:
            vm_push(vm, BOOL_VAL(false));
            break;

        case OP_CONST:
            vm_push(vm, READ_CONSTANT());
            break;

        case OP_GET_LOCAL:
            vm_push(vm, frame->slots[READ_BYTE()]);
            break;

        case OP_SET_LOCAL:
            frame->slots[READ_BYTE()] = vm_peek(vm, 0);
            break;

        case OP_EQL:
            vm_push(vm, BOOL_VAL(values_equal(vm_pop(vm), vm_pop(vm))));
            break;

        case OP_NEQ:
            vm_push(vm, BOOL_VAL(!values_equal(vm_pop(vm), vm_pop(vm))));
            break;

        case OP_NOT:
            vm_push(vm, BOOL_VAL(value_is_falsey(vm_pop(vm))));
            break;

        case OP_RETURN: {
            Value returned = vm_pop(vm);

            vm->frame_count--;

            if (vm->frame_count == 0) {
                *result = returned;

                return true;
            }

            vm->sp = frame->slots;

            vm_push(vm, returned);

            frame = &vm->frames[vm->frame_count - 1];

            break;
        }

        default:
            assert(false && "TODO");
            break;
        }
    }
}

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

        if (((ObjString *)a)->characters == ((ObjString *)b)->characters) {
            return true;
        }

        for (uint32_t i = 0; i < ((ObjString *)a)->count; i++) {
            if (((ObjString *)a)->characters[i] !=
                ((ObjString *)b)->characters[i]) {
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

        if (((ObjMap *)a)->keys == ((ObjMap *)b)->keys &&
            ((ObjMap *)a)->values == ((ObjMap *)b)->values) {
            return true;
        }

        for (uint32_t i = 0; i < ((ObjMap *)a)->count; i++) {
            bool found_match = false;

            for (uint32_t j = 0; j < ((ObjMap *)b)->count; j++) {
                if (values_equal(((ObjMap *)a)->keys[i],
                                 ((ObjMap *)b)->keys[j])) {
                    found_match = true;

                    if (!values_equal(((ObjMap *)a)->values[i],
                                      ((ObjMap *)a)->values[j])) {
                        return false;
                    }
                }
            }

            if (!found_match) {
                return false;
            }
        }

        return true;

    case OBJ_NATIVE:
        return ((ObjNative *)a)->fn == ((ObjNative *)b)->fn;
    }
}

bool values_exactly_equal(Value a, Value b) {
    if (a.tag != b.tag) {
        return false;
    }

    switch (a.tag) {
    case VAL_NONE:
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
    case VAL_NONE:
        return true;

    case VAL_BOOL:
        return AS_BOOL(a) == AS_BOOL(b);

    case VAL_INT:
        if (b.tag == VAL_FLT) {
            return (double)AS_INT(a) == AS_FLT(b);
        } else {
            return AS_INT(a) == AS_INT(b);
        }

    case VAL_FLT:
        if (b.tag == VAL_INT) {
            return AS_FLT(a) == (double)AS_INT(b);
        } else {
            return AS_FLT(a) == AS_FLT(b);
        }

    case VAL_OBJ:
        return objects_equal(AS_OBJ(a), AS_OBJ(b));
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
