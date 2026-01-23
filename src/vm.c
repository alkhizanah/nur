#include <assert.h>
#include <stdarg.h>

#include "array.h"
#include "source_location.h"
#include "vm.h"

static inline bool is_falsey(Value value) {
    return IS_NULL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static inline const char *value_tag_to_string(ValueTag tag) {
    switch (tag) {
    case VAL_NULL:
        return "a null";
    case VAL_BOOL:
        return "a boolean";
    case VAL_INT:
        return "an integer";
    case VAL_FLT:
        return "a floating point";
    case VAL_OBJ:
        return "an object";
    }
}

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

static bool vm_neg(Vm *vm) {
    Value rhs = vm_peek(vm, 0);

    switch (rhs.tag) {
    case VAL_INT:
        vm_poke(vm, 0, INT_VAL(-AS_INT(rhs)));

        return true;

    case VAL_FLT:
        vm_poke(vm, 0, FLT_VAL(-AS_FLT(rhs)));

        return true;

    default:
        vm_error(vm, "can not negate %s value", value_tag_to_string(rhs.tag));

        return false;
    }
}

#define OBJ_ALLOC(vm, tag, type) (type *)vm_alloc(vm, tag, sizeof(tag))

static ObjString *vm_new_string(Vm *vm, size_t reserved) {
    vm->bytes_allocated += sizeof(char) * reserved;

    if (vm->bytes_allocated > vm->next_gc) {
        vm_gc(vm);
    }

    char *items = malloc(sizeof(char) * reserved);

    ObjString *string = OBJ_ALLOC(vm, OBJ_STRING, ObjString);

    string->items = items;
    string->count = 0;
    string->capacity = reserved;

    return string;
}

static ObjString *vm_to_string(Vm *vm, Value value) {
    if (IS_STRING(value)) {
        return AS_STRING(value);
    }

    vm_error(vm, "todo: convert other values to a string");

    exit(1);

    return NULL;
}

static ObjString *vm_string_concat(Vm *vm, ObjString *lhs, ObjString *rhs) {
    ObjString *result = vm_new_string(vm, lhs->count + rhs->count);

    ARRAY_EXPAND(result, lhs->items, lhs->count);
    ARRAY_EXPAND(result, rhs->items, rhs->count);

    return result;
}

static void vm_add_int(Vm *vm, Value lhs, Value rhs) {
    int64_t ilhs = AS_INT(lhs);
    int64_t irhs = AS_INT(rhs);
    vm_pop(vm);
    vm_poke(vm, 0, INT_VAL(ilhs + irhs));
}

static void vm_add_flt(Vm *vm, Value lhs, Value rhs) {
    double flhs = AS_FLT(lhs);
    double frhs = AS_FLT(rhs);
    vm_pop(vm);
    vm_poke(vm, 0, FLT_VAL(flhs + frhs));
}

static void vm_add_int_flt(Vm *vm, Value lhs, Value rhs) {
    int64_t ilhs = AS_INT(lhs);
    double frhs = AS_FLT(rhs);
    vm_pop(vm);
    vm_poke(vm, 0, FLT_VAL(ilhs + frhs));
}

static bool vm_add(Vm *vm) {
    Value rhs = vm_peek(vm, 0);
    Value lhs = vm_peek(vm, 1);

    if (IS_STRING(lhs)) {
        ObjString *slhs = AS_STRING(lhs);
        ObjString *srhs = vm_to_string(vm, rhs);
        ObjString *result = vm_string_concat(vm, slhs, srhs);
        vm_pop(vm);
        vm_poke(vm, 0, OBJ_VAL(result));
        return true;
    } else if (IS_STRING(rhs)) {
        ObjString *slhs = vm_to_string(vm, lhs);
        ObjString *srhs = AS_STRING(rhs);
        ObjString *result = vm_string_concat(vm, slhs, srhs);
        vm_pop(vm);
        vm_poke(vm, 0, OBJ_VAL(result));
        return true;
    }

    if (!IS_INT(lhs) && !IS_FLT(lhs)) {
        vm_error(vm, "can not add to a %s value", value_tag_to_string(lhs.tag));

        return false;
    }

    if (!IS_INT(rhs) && !IS_FLT(rhs)) {
        vm_error(vm, "can not add to a %s value", value_tag_to_string(rhs.tag));

        return false;
    }

    if (lhs.tag == rhs.tag) {
        switch (lhs.tag) {
        case VAL_INT:
            vm_add_int(vm, rhs, lhs);
            break;

        case VAL_FLT:
            vm_add_flt(vm, rhs, lhs);
            break;

        default:
            assert(false && "UNREACHABLE");
        }
    } else {
        switch (lhs.tag) {
        case VAL_INT:
            vm_add_int_flt(vm, lhs, rhs);
            break;

        case VAL_FLT:
            vm_add_int_flt(vm, rhs, lhs);
            break;

        default:
            assert(false && "UNREACHABLE");
        }
    }

    return true;
}

static void vm_sub_int(Vm *vm, Value lhs, Value rhs) {
    int64_t ilhs = AS_INT(lhs);
    int64_t irhs = AS_INT(rhs);
    vm_pop(vm);
    vm_poke(vm, 0, INT_VAL(ilhs - irhs));
}

static void vm_sub_flt(Vm *vm, Value lhs, Value rhs) {
    double flhs = AS_FLT(lhs);
    double frhs = AS_FLT(rhs);
    vm_pop(vm);
    vm_poke(vm, 0, FLT_VAL(flhs - frhs));
}

static void vm_sub_int_flt(Vm *vm, Value lhs, Value rhs) {
    int64_t ilhs = AS_INT(lhs);
    double frhs = AS_FLT(rhs);
    vm_pop(vm);
    vm_poke(vm, 0, FLT_VAL(ilhs - frhs));
}

static void vm_sub_flt_int(Vm *vm, Value lhs, Value rhs) {
    double flhs = AS_FLT(lhs);
    int64_t irhs = AS_INT(rhs);
    vm_pop(vm);
    vm_poke(vm, 0, FLT_VAL(flhs - irhs));
}

static bool vm_sub(Vm *vm) {
    Value rhs = vm_peek(vm, 0);
    Value lhs = vm_peek(vm, 1);

    if (!IS_INT(lhs) && !IS_FLT(lhs)) {
        vm_error(vm, "can not subtract from a %s value",
                 value_tag_to_string(lhs.tag));

        return false;
    }

    if (!IS_INT(rhs) && !IS_FLT(rhs)) {
        vm_error(vm, "can not subtract a %s value",
                 value_tag_to_string(rhs.tag));

        return false;
    }

    if (lhs.tag == rhs.tag) {
        switch (lhs.tag) {
        case VAL_INT:
            vm_sub_int(vm, rhs, lhs);
            break;

        case VAL_FLT:
            vm_sub_flt(vm, rhs, lhs);
            break;

        default:
            assert(false && "UNREACHABLE");
        }
    } else {
        switch (lhs.tag) {
        case VAL_INT:
            vm_sub_int_flt(vm, lhs, rhs);
            break;

        case VAL_FLT:
            vm_sub_flt_int(vm, lhs, rhs);
            break;

        default:
            assert(false && "UNREACHABLE");
        }
    }

    return true;
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
        case OP_NULL:
            vm_push(vm, NULL_VAL);
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
            vm_poke(vm, 0, BOOL_VAL(values_equal(vm_pop(vm), vm_peek(vm, 0))));
            break;

        case OP_NEQ:
            vm_poke(vm, 0, BOOL_VAL(!values_equal(vm_pop(vm), vm_peek(vm, 0))));
            break;

        case OP_NOT:
            vm_poke(vm, 0, BOOL_VAL(is_falsey(vm_peek(vm, 0))));
            break;

        case OP_NEG:
            if (!vm_neg(vm)) {
                return false;
            }

            break;

        case OP_ADD:
            if (!vm_add(vm)) {
                return false;
            }

            break;

        case OP_SUB:
            if (!vm_sub(vm)) {
                return false;
            }

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

void vm_error(Vm *vm, const char *format, ...) {
    va_list args;

    va_start(args, format);

    fprintf(stderr, "error: ");
    vfprintf(stderr, format, args);

    va_end(args);

    for (ssize_t i = vm->frame_count - 1; i >= 0; i--) {
        CallFrame *frame = &vm->frames[i];

        size_t instruction = frame->ip - frame->fn->chunk.bytes - 1;
        size_t source = frame->fn->chunk.sources[instruction];

        SourceLocation loc = source_location_of(
            frame->fn->chunk.file_path, frame->fn->chunk.file_content, source);

        fprintf(stderr, "\tat %s:%u:%u", loc.file_path, loc.line, loc.column);
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

void vm_poke(Vm *vm, size_t distance, Value value) {
    vm->sp[-1 - distance] = value;
}

void vm_gc(Vm *vm) { vm->next_gc = vm->bytes_allocated * VM_GC_GROW_FACTOR; }

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
