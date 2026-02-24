#include <assert.h>
#include <math.h>
#include <stdarg.h>

#include "array.h"
#include "ast.h"
#include "compiler.h"
#include "parser.h"
#include "source_location.h"
#include "vm.h"

void vm_stack_trace(Vm *vm) {
    for (ssize_t i = vm->frame_count - 1; i >= 0; i--) {
        CallFrame *frame = &vm->frames[i];

        size_t instruction = frame->ip - frame->closure->fn->chunk.bytes - 1;
        size_t source = frame->closure->fn->chunk.sources[instruction];

        SourceLocation loc =
            source_location_of(frame->closure->fn->chunk.file_path,
                               frame->closure->fn->chunk.file_content, source);

        fprintf(stderr, "\tat %s:%u:%u\n", loc.file_path, loc.line, loc.column);
    }
}

void vm_error(Vm *vm, const char *format, ...) {
    va_list args;

    va_start(args, format);

    fprintf(stderr, "error: ");
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");

    va_end(args);

    vm_stack_trace(vm);
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

bool vm_load_file(Vm *vm, const char *file_path, const char *file_buffer) {
    Parser parser = {.file_path = file_path, .lexer = {.buffer = file_buffer}};

    AstNodeIdx program = parse(&parser);

    if (program == INVALID_NODE_IDX) {
        return false;
    }

    AstNode block = parser.ast.nodes.items[program];

    CallFrame *frame = &vm->frames[vm->frame_count++];

    ObjFunction *fn = vm_new_function(vm, vm_new_map(vm),
                                      (Chunk){
                                          .file_path = file_path,
                                          .file_content = file_buffer,
                                      },
                                      0, 0);

    frame->closure = vm_new_closure(vm, fn);

    vm_map_insert_builtins(vm, frame->closure->fn->globals);

    frame->slots = vm->stack;

    Compiler compiler = {
        .file_path = file_path,
        .file_buffer = file_buffer,
        .ast = parser.ast,
        .vm = vm,
        .chunk = &frame->closure->fn->chunk,
    };

    if (!compile_block(&compiler, block)) {
        return false;
    }

    free(parser.ast.nodes.items);
    free(parser.ast.nodes.sources);
    free(parser.ast.extra.items);
    free(parser.ast.strings.items);

    chunk_add_byte(compiler.chunk, OP_PUSH_NULL, 0);
    chunk_add_byte(compiler.chunk, OP_RETURN, 0);

    frame->ip = frame->closure->fn->chunk.bytes;

    return true;
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
        vm_error(vm, "can not negate %s value", value_description(rhs));

        return false;
    }
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

static ObjString *vm_to_string(Vm *vm, Value value) {
    if (IS_STRING(value)) {
        return AS_STRING(value);
    }

    vm_error(vm, "todo: convert other values to a string");

    exit(1);

    return NULL;
}

static bool vm_add(Vm *vm) {
    Value rhs = vm_peek(vm, 0);
    Value lhs = vm_peek(vm, 1);

    if (IS_STRING(lhs)) {
        ObjString *slhs = AS_STRING(lhs);
        ObjString *srhs = vm_to_string(vm, rhs);
        ObjString *result = vm_concat_strings(vm, slhs, srhs);
        vm_pop(vm);
        vm_poke(vm, 0, OBJ_VAL(result));

        return true;
    } else if (IS_STRING(rhs)) {
        ObjString *slhs = vm_to_string(vm, lhs);
        ObjString *srhs = AS_STRING(rhs);
        ObjString *result = vm_concat_strings(vm, slhs, srhs);
        vm_pop(vm);
        vm_poke(vm, 0, OBJ_VAL(result));

        return true;
    } else if (IS_ARRAY(lhs)) {
        if (!IS_ARRAY(rhs)) {
            vm_error(vm,
                     "can not add %s value to %s value, did you mean to use "
                     "`array_push`?",
                     value_description(lhs), value_description(rhs));

            return false;
        }

        ObjArray *slhs = AS_ARRAY(lhs);
        ObjArray *srhs = AS_ARRAY(rhs);
        ObjArray *result = vm_concat_arrays(vm, slhs, srhs);
        vm_pop(vm);
        vm_poke(vm, 0, OBJ_VAL(result));

        return true;
    } else if (IS_ARRAY(rhs)) {
        vm_error(vm,
                 "can not add %s value to %s value, did you mean to use "
                 "`array_push`?",
                 value_description(lhs), value_description(rhs));

        return false;
    }

    if ((!IS_INT(lhs) && !IS_FLT(lhs)) || (!IS_INT(rhs) && !IS_FLT(rhs))) {
        vm_error(vm, "can not add %s value to %s value", value_description(lhs),
                 value_description(rhs));

        return false;
    }

    if (lhs.tag == rhs.tag) {
        switch (lhs.tag) {
        case VAL_INT:
            vm_add_int(vm, lhs, rhs);
            break;

        case VAL_FLT:
            vm_add_flt(vm, lhs, rhs);
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
                 value_description(rhs), value_description(lhs));

        return false;
    }

    if (lhs.tag == rhs.tag) {
        switch (lhs.tag) {
        case VAL_INT:
            vm_sub_int(vm, lhs, rhs);
            break;

        case VAL_FLT:
            vm_sub_flt(vm, lhs, rhs);
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
                 value_description(lhs), value_description(rhs));

        return false;
    }

    if (lhs.tag == rhs.tag) {
        switch (lhs.tag) {
        case VAL_INT:
            vm_mul_int(vm, lhs, rhs);
            break;

        case VAL_FLT:
            vm_mul_flt(vm, lhs, rhs);
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
                 value_description(lhs), value_description(rhs));

        return false;
    }

    if (lhs.tag == rhs.tag) {
        switch (lhs.tag) {
        case VAL_INT:
            vm_div_int(vm, lhs, rhs);
            break;

        case VAL_FLT:
            vm_div_flt(vm, lhs, rhs);
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
                 value_description(lhs), value_description(rhs));

        return false;
    }

    if (lhs.tag == rhs.tag) {
        switch (lhs.tag) {
        case VAL_INT:
            vm_mod_int(vm, lhs, rhs);
            break;

        case VAL_FLT:
            vm_mod_flt(vm, lhs, rhs);
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

static bool vm_call_closure(Vm *vm, ObjClosure *closure, uint8_t argc) {
    if (argc != closure->fn->arity) {
        vm_error(vm, "expected %d arguments but got %d instead",
                 closure->fn->arity, argc);

        return false;
    }

    if (vm->frame_count == VM_FRAMES_MAX) {
        vm_error(vm, "stack overflow");

        return false;
    }

    CallFrame *frame = &vm->frames[vm->frame_count++];

    frame->closure = closure;
    frame->ip = closure->fn->chunk.bytes;
    frame->slots = vm->sp - argc;

    return true;
}

static bool vm_call_native(Vm *vm, NativeFn fn, uint8_t argc) {
    Value result;

    if (!fn(vm, vm->sp - argc, argc, &result)) {
        return false;
    }

    vm->sp -= argc;

    vm_push(vm, result);

    return true;
}

static bool vm_call_value(Vm *vm, Value callee, uint8_t argc) {
    if (IS_OBJ(callee)) {
        switch (AS_OBJ(callee)->tag) {
        case OBJ_CLOSURE:
            return vm_call_closure(vm, AS_CLOSURE(callee), argc);

        case OBJ_NATIVE:
            return vm_call_native(vm, AS_NATIVE(callee)->fn, argc);

        case OBJ_FUNCTION:
            assert(false && "UNREACHABLE");

        default:
            break;
        }
    }

    vm_error(vm, "can not call %s value", value_description(callee));

    return false;
}

static bool vm_pow(Vm *vm) {
    Value rhs = vm_peek(vm, 0);
    Value lhs = vm_peek(vm, 1);

    if ((!IS_INT(lhs) && !IS_FLT(lhs)) || (!IS_INT(rhs) && !IS_FLT(rhs))) {
        vm_error(vm, "can not get %s value to the power of %s value",
                 value_description(lhs), value_description(rhs));

        return false;
    }

    if (lhs.tag == rhs.tag) {
        switch (lhs.tag) {
        case VAL_INT:
            vm_pow_int(vm, lhs, rhs);
            break;

        case VAL_FLT:
            vm_pow_flt(vm, lhs, rhs);
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
                     value_description(lhs), value_description(rhs));          \
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

VM_CMP_FN(vm_lt, <);
VM_CMP_FN(vm_gt, >);
VM_CMP_FN(vm_lte, <=);
VM_CMP_FN(vm_gte, >=);

static bool vm_get_subscript(Vm *vm) {
    Value target = vm_pop(vm);
    Value index = vm_pop(vm);

    if (IS_ARRAY(target)) {
        if (!IS_INT(index)) {
            vm_error(vm, "cannot access an array with %s value",
                     value_description(index));

            return false;
        }

        ObjArray *array = AS_ARRAY(target);

        int64_t i = AS_INT(index);

        if (i < 0) {
            i += array->count;
        }

        if (i >= array->count) {
            vm_error(vm,
                     "access out of bounds, array has %d elements "
                     "while the index is %ld",
                     array->count, i);

            return false;
        }

        vm_push(vm, array->items[i]);
    } else if (IS_STRING(target)) {
        if (!IS_INT(index)) {
            vm_error(vm, "cannot access a string with %s value",
                     value_description(index));

            return false;
        }

        ObjString *string = AS_STRING(target);

        uint32_t characters_count = string_utf8_characters_count(
            string->items, string->items + string->count);

        int64_t i = AS_INT(index);

        if (i < 0) {
            i += characters_count;
        }

        if (i >= characters_count) {
            vm_error(vm,
                     "access out of bounds, string has %d characters "
                     "while the index is %ld",
                     characters_count, i);

            return false;
        }

        const char *start = string->items;
        const char *end = string_utf8_skip_character(start);

        for (uint32_t j = 0; j < i; j++) {
            start = string_utf8_skip_character(start);
            end = string_utf8_skip_character(start);
        }

        uint32_t len = end - start;

        ObjString *indexed = vm_copy_string(vm, start, len);

        vm_push(vm, OBJ_VAL(indexed));
    } else if (IS_MAP(target)) {
        if (!IS_STRING(index)) {
            vm_error(vm, "cannot access a map with %s value",
                     value_description(index));

            return false;
        }

        ObjMap *map = AS_MAP(target);

        ObjString *key = AS_STRING(index);

        Value value;

        if (!vm_map_lookup(map, key, &value)) {
            vm_error(vm, "map does not have a '%.*s' key", key->count,
                     key->items);

            return false;
        }

        vm_push(vm, value);
    } else {
        vm_error(vm, "expected an array or a string or a map but got %s",
                 value_description(target));

        return false;
    }

    return true;
}

static bool vm_set_subscript(Vm *vm) {
    Value target = vm_pop(vm);
    Value index = vm_pop(vm);

    if (IS_ARRAY(target)) {
        if (!IS_INT(index)) {
            vm_error(vm, "cannot access an array with %s value",
                     value_description(index));

            return false;
        }

        ObjArray *array = AS_ARRAY(target);

        int64_t i = AS_INT(index);

        if (i < 0) {
            i += array->count;
        }

        if (i >= array->count) {
            vm_error(vm,
                     "access out of bounds, array has %d elements "
                     "while the index is %ld",
                     array->count, i);

            return false;
        }

        array->items[i] = vm_peek(vm, 0);
    } else if (IS_STRING(target)) {
        vm_error(vm, "strings are immutable");

        return false;
    } else if (IS_MAP(target)) {
        if (!IS_STRING(index)) {
            vm_error(vm, "cannot access a map with %s value",
                     value_description(index));

            return false;
        }

        ObjMap *map = AS_MAP(target);

        ObjString *key = AS_STRING(index);

        Value value = vm_peek(vm, 0);

        vm_map_insert(vm, map, key, value);
    } else {
        vm_error(vm, "expected an array or a map but got %s",
                 value_description(target));

        return false;
    }

    return true;
}

static ObjUpvalue *vm_capture_upvalue(Vm *vm, Value *local) {
    ObjUpvalue *prev_upvalue = NULL;
    ObjUpvalue *open_upvalue = vm->open_upvalues;

    while (open_upvalue != NULL && open_upvalue->location > local) {
        prev_upvalue = open_upvalue;
        open_upvalue = open_upvalue->next;
    }

    if (open_upvalue != NULL && open_upvalue->location == local) {
        return open_upvalue;
    }

    ObjUpvalue *new_upvalue = vm_new_upvalue(vm, local);

    new_upvalue->next = open_upvalue;

    if (prev_upvalue == NULL) {
        vm->open_upvalues = new_upvalue;
    } else {
        prev_upvalue->next = new_upvalue;
    }

    return new_upvalue;
}

static void vm_close_upvalues(Vm *vm, Value *last) {
    while (vm->open_upvalues != NULL && vm->open_upvalues->location >= last) {
        ObjUpvalue *upvalue = vm->open_upvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        vm->open_upvalues = upvalue->next;
    }
}

bool vm_run(Vm *vm, Value *result) {
    CallFrame *frame = &vm->frames[vm->frame_count - 1];

#define READ_BYTE() (*frame->ip++)

#define READ_SHORT()                                                           \
    (frame->ip += 2, ((uint16_t)frame->ip[-2] << 8) | frame->ip[-1])

#define READ_WORD()                                                            \
    (frame->ip += 4, ((uint32_t)frame->ip[-4] << 24) |                         \
                         ((uint32_t)frame->ip[-3] << 16) |                     \
                         ((uint32_t)frame->ip[-2] << 8) | frame->ip[-1])

#define READ_CONSTANT()                                                        \
    (frame->closure->fn->chunk.constants.items[READ_SHORT()])

#define READ_STRING() (AS_STRING(READ_CONSTANT()))

    for (;;) {
        OpCode opcode = READ_BYTE();

        switch (opcode) {
        case OP_POP:
            vm_pop(vm);
            break;

        case OP_DUP:
            vm_push(vm, vm_peek(vm, 0));
            break;

        case OP_SWP: {
            Value a = vm_peek(vm, 0);
            Value b = vm_peek(vm, 1);
            vm_poke(vm, 0, b);
            vm_poke(vm, 1, a);
            break;
        }

        case OP_PUSH_NULL:
            vm_push(vm, NULL_VAL);
            break;

        case OP_PUSH_TRUE:
            vm_push(vm, BOOL_VAL(true));
            break;

        case OP_PUSH_FALSE:
            vm_push(vm, BOOL_VAL(false));
            break;

        case OP_PUSH_CONST:
            vm_push(vm, READ_CONSTANT());
            break;

        case OP_GET_LOCAL:
            vm_push(vm, frame->slots[READ_BYTE()]);
            break;

        case OP_SET_LOCAL:
            frame->slots[READ_BYTE()] = vm_peek(vm, 0);
            break;

        case OP_GET_UPVALUE:
            vm_push(vm, *frame->closure->upvalues[READ_BYTE()]->location);
            break;

        case OP_SET_UPVALUE:
            *frame->closure->upvalues[READ_BYTE()]->location = vm_peek(vm, 0);
            break;

        case OP_CLOSE_UPVALUE:
            vm_close_upvalues(vm, vm->sp - 1);
            vm_pop(vm);
            break;

        case OP_GET_GLOBAL: {
            ObjString *key = READ_STRING();

            Value value;

            if (!vm_map_lookup(frame->closure->fn->globals, key, &value)) {
                vm_error(vm,
                         "'%.*s' is not "
                         "defined",
                         (int)key->count, key->items);

                return false;
            }

            vm_push(vm, value);

            break;
        }

        case OP_SET_GLOBAL:
            vm_map_insert(vm, frame->closure->fn->globals, READ_STRING(),
                          vm_peek(vm, 0));
            break;

        case OP_GET_SUBSCRIPT:
            if (!vm_get_subscript(vm)) {
                return false;
            }

            break;

        case OP_SET_SUBSCRIPT:
            if (!vm_set_subscript(vm)) {
                return false;
            }

            break;

        case OP_MAKE_ARRAY: {
            uint32_t count = READ_WORD();

            ObjArray *array = vm_copy_array(vm, vm->sp - count, count);

            vm->sp -= count;

            vm_push(vm, OBJ_VAL(array));

            break;
        }

        case OP_COPY_ARRAY: {
            Value target = vm_pop(vm);

            if (!IS_ARRAY(target)) {
                vm_error(vm, "%s is not an array value",
                         value_description(target));
            }

            ObjArray *array = AS_ARRAY(target);

            vm_push(vm, OBJ_VAL(vm_copy_array(vm, array->items, array->count)));

            break;
        }

        case OP_MAKE_SLICE: {
            Value target = vm_pop(vm);

            if (!IS_ARRAY(target)) {
                vm_error(vm, "%s is not an array value",
                         value_description(target));
            }

            ObjArray *array = AS_ARRAY(target);

            Value start = vm_pop(vm);

            if (!IS_INT(start)) {
                vm_error(vm, "cannot access an array with %s value",
                         value_description(start));

                return false;
            }

            Value end = vm_pop(vm);

            if (!IS_INT(end)) {
                vm_error(vm, "cannot access an array with %s value",
                         value_description(end));

                return false;
            }

            int64_t istart = AS_INT(start);

            if (istart >= array->count) {
                vm_error(
                    vm,
                    "sliced array has %d elements, the slice start must be "
                    "less than that, but got %ld",
                    array->count, istart);

                return false;
            }

            int64_t iend = AS_INT(end);

            if (iend > array->count) {
                vm_error(vm,
                         "sliced array has %d elements, the slice end must be "
                         "less than that or equal to it, but got %ld",
                         array->count, iend);

                return false;
            }

            vm_push(vm, OBJ_VAL(vm_copy_array(vm, array->items + istart,
                                              iend - istart)));

            break;
        }

        case OP_MAKE_SLICE_UNDER: {
            Value target = vm_pop(vm);

            if (!IS_ARRAY(target)) {
                vm_error(vm, "%s is not an array value",
                         value_description(target));
            }

            ObjArray *array = AS_ARRAY(target);

            Value end = vm_pop(vm);

            if (!IS_INT(end)) {
                vm_error(vm, "cannot access an array with %s value",
                         value_description(end));

                return false;
            }

            int64_t iend = AS_INT(end);

            if (iend > array->count) {
                vm_error(vm,
                         "sliced array has %d elements, the slice end must be "
                         "less than that or equal to it, but got %ld",
                         array->count, iend);

                return false;
            }

            vm_push(vm, OBJ_VAL(vm_copy_array(vm, array->items,
                                              iend)));

            break;
        }

        case OP_MAKE_SLICE_ABOVE: {
            Value target = vm_pop(vm);

            if (!IS_ARRAY(target)) {
                vm_error(vm, "%s is not an array value",
                         value_description(target));
            }

            ObjArray *array = AS_ARRAY(target);

            Value start = vm_pop(vm);

            if (!IS_INT(start)) {
                vm_error(vm, "cannot access an array with %s value",
                         value_description(start));

                return false;
            }

            int64_t istart = AS_INT(start);

            if (istart >= array->count) {
                vm_error(
                    vm,
                    "sliced array has %d elements, the slice start must be "
                    "less than that, but got %ld",
                    array->count, istart);

                return false;
            }

            vm_push(vm, OBJ_VAL(vm_copy_array(vm, array->items + istart,
                                              array->count - istart)));

            break;
        }

        case OP_MAKE_MAP: {
            uint32_t count = READ_WORD();

            ObjMap *map = vm_new_map(vm);

            Value *start = vm->sp - count * 2;

            for (uint32_t i = 0; i < count * 2; i += 2) {
                if (!IS_STRING(start[i])) {
                    vm_error(vm,
                             "expected a "
                             "string got %s",
                             value_description(start[i]));

                    return false;
                }

                ObjString *key = AS_STRING(start[i]);
                Value value = start[i + 1];

                vm_map_insert(vm, map, key, value);
            }

            vm->sp = start;

            vm_push(vm, OBJ_VAL(map));

            break;
        }

        case OP_MAKE_CLOSURE: {
            ObjFunction *fn = AS_FUNCTION(READ_CONSTANT());

            fn->globals = frame->closure->fn->globals;

            ObjClosure *closure = vm_new_closure(vm, fn);

            for (uint8_t i = 0; i < fn->upvalues_count; i++) {
                uint8_t is_local = READ_BYTE();
                uint8_t index = READ_BYTE();

                if (is_local) {
                    closure->upvalues[i] =
                        vm_capture_upvalue(vm, frame->slots + index);
                } else {
                    closure->upvalues[i] = frame->closure->upvalues[index];
                }
            }

            vm_push(vm, OBJ_VAL(closure));

            break;
        }

        case OP_EQL:
            vm_push(vm, BOOL_VAL(values_equal(vm_pop(vm), vm_pop(vm))));
            break;

        case OP_NEQ:
            vm_push(vm, BOOL_VAL(!values_equal(vm_pop(vm), vm_pop(vm))));
            break;

        case OP_NOT:
            vm_poke(vm, 0, BOOL_VAL(value_is_falsey(vm_peek(vm, 0))));
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

        case OP_CALL:
            if (!vm_call_value(vm, vm_pop(vm), READ_BYTE())) {
                return false;
            }

            frame = &vm->frames[vm->frame_count - 1];

            break;

        case OP_POP_JUMP_IF_FALSE: {
            uint16_t offset = READ_SHORT();

            if (value_is_falsey(vm_pop(vm)))
                frame->ip += offset;

            break;
        }

        case OP_JUMP_IF_FALSE: {
            uint16_t offset = READ_SHORT();

            if (value_is_falsey(vm_peek(vm, 0)))
                frame->ip += offset;

            break;
        }

        case OP_JUMP: {
            uint16_t offset = READ_SHORT();

            frame->ip += offset;

            break;
        }

        case OP_LOOP: {
            uint16_t offset = READ_SHORT();

            frame->ip -= offset;

            break;
        }

        case OP_RETURN: {
            Value returned = vm_pop(vm);

            vm_close_upvalues(vm, frame->slots);

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
