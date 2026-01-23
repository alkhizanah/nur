#include <assert.h>
#include <math.h>
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

void vm_stack_reset(Vm *vm) {
    vm->sp = vm->stack;
    vm->frame_count = 0;
}

void vm_init(Vm *vm) {
    vm_stack_reset(vm);

    vm->objects = NULL;
    vm->bytes_allocated = 0;
    vm->next_gc = 1024 * 1024;
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

ObjFunction *vm_new_function(Vm *vm, Chunk chunk, uint8_t arity) {
    ObjFunction *function = OBJ_ALLOC(vm, OBJ_FUNCTION, ObjFunction);

    function->chunk = chunk;
    function->arity = arity;

    return function;
}

ObjString *vm_new_string(Vm *vm, size_t reserved_characters) {
    vm->bytes_allocated += sizeof(char) * reserved_characters;

    if (vm->bytes_allocated > vm->next_gc) {
        vm_gc(vm);
    }

    char *items = malloc(sizeof(char) * reserved_characters);

    ObjString *string = OBJ_ALLOC(vm, OBJ_STRING, ObjString);

    string->items = items;
    string->count = 0;
    string->capacity = reserved_characters;

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

    if ((!IS_INT(lhs) && !IS_FLT(lhs)) || (!IS_INT(rhs) && !IS_FLT(rhs))) {
        vm_error(vm, "can not add %s value to %s value",
                 value_tag_to_string(lhs.tag), value_tag_to_string(rhs.tag));

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

    if ((!IS_INT(lhs) && !IS_FLT(lhs)) || (!IS_INT(rhs) && !IS_FLT(rhs))) {
        vm_error(vm, "can not subtract %s value from %s value",
                 value_tag_to_string(rhs.tag), value_tag_to_string(lhs.tag));

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

static void vm_mul_int(Vm *vm, Value lhs, Value rhs) {
    int64_t ilhs = AS_INT(lhs);
    int64_t irhs = AS_INT(rhs);
    vm_pop(vm);
    vm_poke(vm, 0, INT_VAL(ilhs * irhs));
}

static void vm_mul_flt(Vm *vm, Value lhs, Value rhs) {
    double flhs = AS_FLT(lhs);
    double frhs = AS_FLT(rhs);
    vm_pop(vm);
    vm_poke(vm, 0, FLT_VAL(flhs * frhs));
}

static void vm_mul_int_flt(Vm *vm, Value lhs, Value rhs) {
    int64_t ilhs = AS_INT(lhs);
    double frhs = AS_FLT(rhs);
    vm_pop(vm);
    vm_poke(vm, 0, FLT_VAL(ilhs * frhs));
}

static bool vm_mul(Vm *vm) {
    Value rhs = vm_peek(vm, 0);
    Value lhs = vm_peek(vm, 1);

    if ((!IS_INT(lhs) && !IS_FLT(lhs)) || (!IS_INT(rhs) && !IS_FLT(rhs))) {
        vm_error(vm, "can not multiply %s value with %s value",
                 value_tag_to_string(lhs.tag), value_tag_to_string(rhs.tag));

        return false;
    }

    if (lhs.tag == rhs.tag) {
        switch (lhs.tag) {
        case VAL_INT:
            vm_mul_int(vm, rhs, lhs);
            break;

        case VAL_FLT:
            vm_mul_flt(vm, rhs, lhs);
            break;

        default:
            assert(false && "UNREACHABLE");
        }
    } else {
        switch (lhs.tag) {
        case VAL_INT:
            vm_mul_int_flt(vm, lhs, rhs);
            break;

        case VAL_FLT:
            vm_mul_int_flt(vm, rhs, lhs);
            break;

        default:
            assert(false && "UNREACHABLE");
        }
    }

    return true;
}

static int64_t rem_euclid(int64_t a, int64_t b) {
    int64_t r = a % b;
    return r < 0 ? r + (b < 0 ? -b : b) : r;
}

static double frem_euclid(double a, double b) {
    double r = fmod(a, b);
    return r < 0 ? r + (b < 0 ? -b : b) : r;
}

static void vm_div_int(Vm *vm, Value lhs, Value rhs) {
    int64_t ilhs = AS_INT(lhs);
    int64_t irhs = AS_INT(rhs);

    vm_pop(vm);

    if (rem_euclid(ilhs, irhs) == 0) {
        vm_poke(vm, 0, INT_VAL(ilhs / irhs));
    } else {
        vm_poke(vm, 0, FLT_VAL((double)ilhs / (double)irhs));
    }
}

static void vm_div_flt(Vm *vm, Value lhs, Value rhs) {
    double flhs = AS_FLT(lhs);
    double frhs = AS_FLT(rhs);
    vm_pop(vm);
    vm_poke(vm, 0, FLT_VAL(flhs / frhs));
}

static void vm_div_int_flt(Vm *vm, Value lhs, Value rhs) {
    int64_t ilhs = AS_INT(lhs);
    double frhs = AS_FLT(rhs);
    vm_pop(vm);
    vm_poke(vm, 0, FLT_VAL(ilhs / frhs));
}

static void vm_div_flt_int(Vm *vm, Value lhs, Value rhs) {
    double flhs = AS_FLT(lhs);
    int64_t irhs = AS_INT(rhs);
    vm_pop(vm);
    vm_poke(vm, 0, FLT_VAL(flhs / irhs));
}

static bool vm_div(Vm *vm) {
    Value rhs = vm_peek(vm, 0);
    Value lhs = vm_peek(vm, 1);

    if ((!IS_INT(lhs) && !IS_FLT(lhs)) || (!IS_INT(rhs) && !IS_FLT(rhs))) {
        vm_error(vm, "can not divide %s value by %s value",
                 value_tag_to_string(lhs.tag), value_tag_to_string(rhs.tag));

        return false;
    }

    if (lhs.tag == rhs.tag) {
        switch (lhs.tag) {
        case VAL_INT:
            vm_div_int(vm, rhs, lhs);
            break;

        case VAL_FLT:
            vm_div_flt(vm, rhs, lhs);
            break;

        default:
            assert(false && "UNREACHABLE");
        }
    } else {
        switch (lhs.tag) {
        case VAL_INT:
            vm_div_int_flt(vm, lhs, rhs);
            break;

        case VAL_FLT:
            vm_div_flt_int(vm, lhs, rhs);
            break;

        default:
            assert(false && "UNREACHABLE");
        }
    }

    return true;
}

static void vm_mod_int(Vm *vm, Value lhs, Value rhs) {
    int64_t ilhs = AS_INT(lhs);
    int64_t irhs = AS_INT(rhs);
    vm_pop(vm);
    vm_poke(vm, 0, INT_VAL(rem_euclid(ilhs, irhs)));
}

static void vm_mod_flt(Vm *vm, Value lhs, Value rhs) {
    double flhs = AS_FLT(lhs);
    double frhs = AS_FLT(rhs);
    vm_pop(vm);
    vm_poke(vm, 0, FLT_VAL(frem_euclid(flhs, frhs)));
}

static void vm_mod_int_flt(Vm *vm, Value lhs, Value rhs) {
    int64_t ilhs = AS_INT(lhs);
    double frhs = AS_FLT(rhs);
    vm_pop(vm);
    vm_poke(vm, 0, FLT_VAL(frem_euclid(ilhs, frhs)));
}

static void vm_mod_flt_int(Vm *vm, Value lhs, Value rhs) {
    double flhs = AS_FLT(lhs);
    int64_t irhs = AS_INT(rhs);
    vm_pop(vm);
    vm_poke(vm, 0, FLT_VAL(frem_euclid(flhs, irhs)));
}

static bool vm_mod(Vm *vm) {
    Value rhs = vm_peek(vm, 0);
    Value lhs = vm_peek(vm, 1);

    if ((!IS_INT(lhs) && !IS_FLT(lhs)) || (!IS_INT(rhs) && !IS_FLT(rhs))) {
        vm_error(vm, "can not get %s value modulo %s value",
                 value_tag_to_string(lhs.tag), value_tag_to_string(rhs.tag));

        return false;
    }

    if (lhs.tag == rhs.tag) {
        switch (lhs.tag) {
        case VAL_INT:
            vm_mod_int(vm, rhs, lhs);
            break;

        case VAL_FLT:
            vm_mod_flt(vm, rhs, lhs);
            break;

        default:
            assert(false && "UNREACHABLE");
        }
    } else {
        switch (lhs.tag) {
        case VAL_INT:
            vm_mod_int_flt(vm, lhs, rhs);
            break;

        case VAL_FLT:
            vm_mod_flt_int(vm, lhs, rhs);
            break;

        default:
            assert(false && "UNREACHABLE");
        }
    }

    return true;
}

static void vm_pow_int(Vm *vm, Value lhs, Value rhs) {
    int64_t ilhs = AS_INT(lhs);
    int64_t irhs = AS_INT(rhs);
    vm_pop(vm);

    double result = pow(ilhs, irhs);

    if (floor(result) == result) {
        vm_poke(vm, 0, INT_VAL(result));
    } else {
        vm_poke(vm, 0, FLT_VAL(result));
    }
}

static void vm_pow_flt(Vm *vm, Value lhs, Value rhs) {
    double flhs = AS_FLT(lhs);
    double frhs = AS_FLT(rhs);
    vm_pop(vm);
    vm_poke(vm, 0, FLT_VAL(pow(flhs, frhs)));
}

static void vm_pow_int_flt(Vm *vm, Value lhs, Value rhs) {
    int64_t ilhs = AS_INT(lhs);
    double frhs = AS_FLT(rhs);
    vm_pop(vm);
    vm_poke(vm, 0, FLT_VAL(pow(ilhs, frhs)));
}

static void vm_pow_flt_int(Vm *vm, Value lhs, Value rhs) {
    double flhs = AS_FLT(lhs);
    int64_t irhs = AS_INT(rhs);
    vm_pop(vm);
    vm_poke(vm, 0, FLT_VAL(pow(flhs, irhs)));
}

static bool vm_pow(Vm *vm) {
    Value rhs = vm_peek(vm, 0);
    Value lhs = vm_peek(vm, 1);

    if ((!IS_INT(lhs) && !IS_FLT(lhs)) || (!IS_INT(rhs) && !IS_FLT(rhs))) {
        vm_error(vm, "can not get %s value to the power of %s value",
                 value_tag_to_string(lhs.tag), value_tag_to_string(rhs.tag));

        return false;
    }

    if (lhs.tag == rhs.tag) {
        switch (lhs.tag) {
        case VAL_INT:
            vm_pow_int(vm, rhs, lhs);
            break;

        case VAL_FLT:
            vm_pow_flt(vm, rhs, lhs);
            break;

        default:
            assert(false && "UNREACHABLE");
        }
    } else {
        switch (lhs.tag) {
        case VAL_INT:
            vm_pow_int_flt(vm, lhs, rhs);
            break;

        case VAL_FLT:
            vm_pow_flt_int(vm, lhs, rhs);
            break;

        default:
            assert(false && "UNREACHABLE");
        }
    }

    return true;
}

#define VM_CMP_FN(name, op)                                                    \
    static bool name(Vm *vm) {                                                 \
        Value rhs = vm_peek(vm, 0);                                            \
        Value lhs = vm_peek(vm, 1);                                            \
                                                                               \
        if ((!IS_INT(lhs) && !IS_FLT(lhs)) ||                                  \
            (!IS_INT(rhs) && !IS_FLT(rhs))) {                                  \
            vm_error(vm, "can not compare %s value with %s value",             \
                     value_tag_to_string(lhs.tag),                             \
                     value_tag_to_string(rhs.tag));                            \
                                                                               \
            return false;                                                      \
        }                                                                      \
                                                                               \
        vm_pop(vm);                                                            \
                                                                               \
        if (IS_INT(lhs)) {                                                     \
            if (IS_INT(rhs)) {                                                 \
                vm_poke(vm, 0, BOOL_VAL(AS_INT(lhs) op AS_INT(rhs)));          \
            } else {                                                           \
                vm_poke(vm, 0, BOOL_VAL(AS_INT(lhs) op AS_FLT(rhs)));          \
            }                                                                  \
        } else if (IS_INT(rhs)) {                                              \
            vm_poke(vm, 0, BOOL_VAL(AS_FLT(lhs) op AS_INT(rhs)));              \
        } else {                                                               \
            vm_poke(vm, 0, BOOL_VAL(AS_FLT(lhs) op AS_FLT(rhs)));              \
        }                                                                      \
                                                                               \
        return true;                                                           \
    }

VM_CMP_FN(vm_lt, <)
VM_CMP_FN(vm_gt, >)
VM_CMP_FN(vm_lte, <=)
VM_CMP_FN(vm_gte, >=)

bool vm_run(Vm *vm, Value *result) {
    CallFrame *frame = &vm->frames[vm->frame_count - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_SHORT() (((uint16_t)READ_BYTE() << 8) | READ_BYTE())
#define READ_CONSTANT() (frame->fn->chunk.constants.items[READ_SHORT()])
#define READ_STRING() (AS_STRING(READ_CONSTANT()))

    for (;;) {
        OpCode opcode = READ_BYTE();

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

        case OP_POP:
            vm_pop(vm);
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

        case OP_MUL:
            if (!vm_mul(vm)) {
                return false;
            }

            break;

        case OP_DIV:
            if (!vm_div(vm)) {
                return false;
            }

            break;

        case OP_MOD:
            if (!vm_mod(vm)) {
                return false;
            }

            break;

        case OP_POW:
            if (!vm_pow(vm)) {
                return false;
            }

            break;

        case OP_LT:
            if (!vm_lt(vm)) {
                return false;
            }

            break;

        case OP_GT:
            if (!vm_gt(vm)) {
                return false;
            }

            break;

        case OP_LTE:
            if (!vm_lte(vm)) {
                return false;
            }

            break;

        case OP_GTE:
            if (!vm_gte(vm)) {
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

static void vm_mark_value(Vm *vm, Value value);

static void vm_mark_object(Vm *vm, Obj *obj) {
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

        for (size_t i = 0; i < map->count; i++) {
            vm_mark_value(vm, map->keys[i]);
            vm_mark_value(vm, map->values[i]);
        }

        break;
    }

    case OBJ_STRING:
    case OBJ_FUNCTION:
    case OBJ_NATIVE:
        break;
    }
}

static void vm_mark_value(Vm *vm, Value value) {
    if (!IS_OBJ(value)) {
        vm_mark_object(vm, AS_OBJ(value));
    }
}

static void vm_mark_roots(Vm *vm) {
    for (Value *v = vm->stack; v < vm->sp; v++) {
        vm_mark_value(vm, *v);
    }

    for (size_t i = 0; i < vm->frame_count; i++) {
        CallFrame frame = vm->frames[i];

        for (size_t j = 0; j < frame.fn->chunk.constants.count; j++) {
            vm_mark_value(vm, frame.fn->chunk.constants.items[j]);
        }
    }
}

static void vm_free_value(Vm *vm, Value value);

static void vm_free_object(Vm *vm, Obj *obj) {
    switch (obj->tag) {
    case OBJ_STRING: {
        ObjString *str = (ObjString *)obj;

        free(str->items);

        vm->bytes_allocated -= str->capacity * sizeof(char) + sizeof(ObjString);

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
        ObjMap *map = (ObjMap *)obj;

        for (size_t i = 0; i < map->count; i++) {
            vm_free_value(vm, map->keys[i]);
            vm_free_value(vm, map->values[i]);
        }

        free(map->keys);
        free(map->values);

        vm->bytes_allocated -=
            map->capacity * 2 * sizeof(Value) + sizeof(ObjMap);

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

    case OBJ_NATIVE:
        break;
    }

    free(obj);
}

static void vm_free_value(Vm *vm, Value value) {
    if (IS_OBJ(value)) {
        vm_free_object(vm, AS_OBJ(value));
    }
}

static void vm_sweep(Vm *vm) {
    Obj *prev = NULL;
    Obj *curr = vm->objects;

    while (curr != NULL) {
        if (curr->marked) {
            curr->marked = false;
            prev = curr;
            curr = curr->next;
        } else {
            Obj *unreached = curr;

            if (prev != NULL) {
                prev->next = curr->next;
            } else {
                vm->objects = curr->next;
            }

            curr = curr->next;

            vm_free_object(vm, unreached);
        }
    }
}

void vm_gc(Vm *vm) {
    vm_mark_roots(vm);

    vm_sweep(vm);

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
