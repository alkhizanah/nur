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

    vm->strings = vm_new_map(vm);
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

    chunk_optimize(compiler.chunk);

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

    if (IS_NUM(rhs)) {
        vm_poke(vm, 0, NUM_VAL(-AS_NUM(rhs)));

        return true;
    }

    vm_error(vm, "can not negate %s value", value_description(rhs));

    return false;
}

ObjString *vm_value_to_string(Vm *vm, Value value) {
    if (IS_STRING(value)) {
        return AS_STRING(value);
    }

    if (IS_NUM(value)) {
        char buffer[1024];
        int len = snprintf(buffer, sizeof(buffer), "%.14g", AS_NUM(value));
        return vm_copy_string(vm, buffer, len);
    }

    if (IS_BOOL(value)) {
        if (AS_BOOL(value)) {
            return vm_copy_string(vm, "true", 4);
        } else {
            return vm_copy_string(vm, "false", 5);
        }
    }

    if (IS_NULL(value)) {
        return vm_copy_string(vm, "null", 4);
    }

    vm_error(vm, "cannot convert value to string");
    exit(1);
}

static bool vm_add(Vm *vm) {
    Value rhs = vm_peek(vm, 0);
    Value lhs = vm_peek(vm, 1);

    if (IS_NUM(lhs) && IS_NUM(rhs)) {
        vm_pop(vm);
        vm_poke(vm, 0, NUM_VAL(AS_NUM(lhs) + AS_NUM(rhs)));
        return true;
    }

    if (IS_STRING(lhs)) {
        ObjString *slhs = AS_STRING(lhs);
        ObjString *srhs = vm_value_to_string(vm, rhs);
        ObjString *result = vm_concat_strings(vm, slhs, srhs);
        vm_pop(vm);
        vm_poke(vm, 0, OBJ_VAL(result));

        return true;
    }

    if (IS_STRING(rhs)) {
        ObjString *slhs = vm_value_to_string(vm, lhs);
        ObjString *srhs = AS_STRING(rhs);
        ObjString *result = vm_concat_strings(vm, slhs, srhs);
        vm_pop(vm);
        vm_poke(vm, 0, OBJ_VAL(result));

        return true;
    }

    if (IS_ARRAY(lhs)) {
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
    }

    if (IS_ARRAY(rhs)) {
        vm_error(vm,
                 "can not add %s value to %s value, did you mean to use "
                 "`array_push`?",
                 value_description(lhs), value_description(rhs));

        return false;
    }

    vm_error(vm, "can not add %s value to %s value", value_description(lhs),
             value_description(rhs));

    return false;
}

static inline bool vm_sub(Vm *vm) {
    Value rhs = vm_peek(vm, 0);
    Value lhs = vm_peek(vm, 1);

    if (IS_NUM(lhs) && IS_NUM(rhs)) {
        vm_pop(vm);
        vm_poke(vm, 0, NUM_VAL(AS_NUM(lhs) - AS_NUM(rhs)));
        return true;
    }

    vm_error(vm, "can not subtract %s value from %s value",
             value_description(rhs), value_description(lhs));

    return false;
}

static inline bool vm_mul(Vm *vm) {
    Value rhs = vm_peek(vm, 0);
    Value lhs = vm_peek(vm, 1);

    if (IS_NUM(lhs) && IS_NUM(rhs)) {
        vm_pop(vm);
        vm_poke(vm, 0, NUM_VAL(AS_NUM(lhs) * AS_NUM(rhs)));
        return true;
    }

    vm_error(vm, "can not multiply %s value with %s value",
             value_description(lhs), value_description(rhs));

    return false;
}

static inline double rem_euclid(double a, double b) {
    double r = fmod(a, b);
    return r < 0 ? r + (b < 0 ? -b : b) : r;
}

static inline bool vm_div(Vm *vm) {
    Value rhs = vm_peek(vm, 0);
    Value lhs = vm_peek(vm, 1);

    if (IS_NUM(lhs) && IS_NUM(rhs)) {
        vm_pop(vm);
        vm_poke(vm, 0, NUM_VAL(AS_NUM(lhs) / AS_NUM(rhs)));
        return true;
    }

    vm_error(vm, "can not divide %s value by %s value", value_description(lhs),
             value_description(rhs));

    return false;
}

static inline bool vm_mod(Vm *vm) {
    Value rhs = vm_peek(vm, 0);
    Value lhs = vm_peek(vm, 1);

    if (IS_NUM(lhs) && IS_NUM(rhs)) {
        vm_pop(vm);
        vm_poke(vm, 0, NUM_VAL(rem_euclid(AS_NUM(lhs), AS_NUM(rhs))));
        return true;
    }

    vm_error(vm, "can not get %s value modulo %s value", value_description(lhs),
             value_description(rhs));

    return false;
}

static inline bool vm_pow(Vm *vm) {
    Value rhs = vm_peek(vm, 0);
    Value lhs = vm_peek(vm, 1);

    if (IS_NUM(lhs) && IS_NUM(rhs)) {
        vm_pop(vm);
        vm_poke(vm, 0, NUM_VAL(pow(AS_NUM(lhs), AS_NUM(rhs))));
        return true;
    }

    vm_error(vm, "can not get %s value to the power of %s value",
             value_description(lhs), value_description(rhs));

    return false;
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

#define VM_CMP_FN(name, op)                                                    \
    static bool name(Vm *vm) {                                                 \
        Value rhs = vm_peek(vm, 0);                                            \
        Value lhs = vm_peek(vm, 1);                                            \
                                                                               \
        if (IS_NUM(lhs) && IS_NUM(rhs)) {                                      \
            vm_pop(vm);                                                        \
            vm_poke(vm, 0, BOOL_VAL(AS_NUM(lhs) op AS_NUM(rhs)));              \
            return true;                                                       \
        }                                                                      \
                                                                               \
        vm_error(vm, "can not compare %s value with %s value",                 \
                 value_description(lhs), value_description(rhs));              \
                                                                               \
        return false;                                                          \
    }

VM_CMP_FN(vm_lt, <);
VM_CMP_FN(vm_gt, >);
VM_CMP_FN(vm_lte, <=);
VM_CMP_FN(vm_gte, >=);

static bool vm_get_subscript(Vm *vm, Value target, Value index) {
    if (IS_ARRAY(target)) {
        if (!IS_NUM(index)) {
            vm_error(vm, "cannot access an array with %s value",
                     value_description(index));

            return false;
        }

        ObjArray *array = AS_ARRAY(target);

        double i = AS_NUM(index);

        if (i < 0) {
            i += array->count;
        }

        if (i >= array->count || (i - floor(i)) != 0) {
            vm_error(vm,
                     "access out of bounds, array has %d elements "
                     "while the index is %g",
                     array->count, i);

            return false;
        }

        vm_push(vm, array->items[(size_t)i]);
    } else if (IS_STRING(target)) {
        if (!IS_NUM(index)) {
            vm_error(vm, "cannot access a string with %s value",
                     value_description(index));

            return false;
        }

        ObjString *string = AS_STRING(target);

        uint32_t characters_count = string_utf8_characters_count(
            string->items, string->items + string->count);

        double i = AS_NUM(index);

        if (i < 0) {
            i += characters_count;
        }

        if (i >= characters_count || (i - floor(i)) != 0) {
            vm_error(vm,
                     "access out of bounds, string has %d characters "
                     "while the index is %g",
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

static bool vm_set_subscript(Vm *vm, Value target, Value index) {
    if (IS_ARRAY(target)) {
        if (!IS_NUM(index)) {
            vm_error(vm, "cannot access an array with %s value",
                     value_description(index));

            return false;
        }

        ObjArray *array = AS_ARRAY(target);

        double i = AS_NUM(index);

        if (i < 0) {
            i += array->count;
        }

        if (i >= array->count || (i - floor(i)) != 0) {
            vm_error(vm,
                     "access out of bounds, array has %d elements "
                     "while the index is %g",
                     array->count, i);

            return false;
        }

        array->items[(size_t)i] = vm_peek(vm, 0);
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

static bool vm_make_slice(Vm *vm) {
    Value target = vm_pop(vm);

    Value start = vm_pop(vm);

    if (!IS_NUM(start)) {
        vm_error(vm, "cannot slice using %s value, expected a number",
                 value_description(start));

        return false;
    }

    Value end = vm_pop(vm);

    if (!IS_NUM(end)) {
        vm_error(vm, "cannot slice using %s value, expected a number",
                 value_description(end));

        return false;
    }

    double fstart = AS_NUM(start);
    double fend = AS_NUM(end);

    if (IS_ARRAY(target)) {
        ObjArray *array = AS_ARRAY(target);

        if (fstart < 0) {
            fstart += array->count;
        }

        if (fend < 0) {
            fend += array->count;
        }

        if (fstart < 0 || fstart >= array->count ||
            (fstart - floor(fstart)) != 0) {
            vm_error(vm,
                     "sliced array has %d elements, the slice start must be an "
                     "integer less than that and greater than zero, but got %g",
                     array->count, fstart);

            return false;
        }

        if (fend < 0 || fend > array->count || (fend - floor(fend)) != 0) {
            vm_error(vm,
                     "sliced array has %d elements, the slice end must be an "
                     "integer less than that and greater than zero, but got %g",
                     array->count, fend);

            return false;
        }

        if (fstart > fend) {
            vm_error(
                vm,
                "slicing start must be less than or equal to slicing end, got "
                "%g as a start and %g as an end",
                fstart, fend);

            return false;
        }

        vm_push(vm, OBJ_VAL(vm_copy_array(vm, array->items + (size_t)fstart,
                                          fend - fstart)));
    } else if (IS_STRING(target)) {
        ObjString *string = AS_STRING(target);

        int64_t characters_count = string_utf8_characters_count(
            string->items, string->items + string->count);

        if (fstart < 0) {
            fstart += characters_count;
        }

        if (fend < 0) {
            fend += characters_count;
        }

        if (fstart < 0 || fstart >= characters_count ||
            (fstart - floor(fstart)) != 0) {
            vm_error(
                vm,
                "sliced string has %ld characters, the slice start must be "
                "an integer less than that and greater than zero, but got %g",
                characters_count, fstart);

            return false;
        }

        if (fend < 0 || fend > characters_count || (fend - floor(fend)) != 0) {
            vm_error(
                vm,
                "sliced string has %ld characters, the slice end must be "
                "an integer less than that and greater than zero, but got %g",
                characters_count, fend);

            return false;
        }

        if (fstart > fend) {
            vm_error(
                vm,
                "slicing start must be less than or equal to slicing end, got "
                "%g as a start and %g as an end",
                fstart, fend);

            return false;
        }

        const char *b = string->items;

        int64_t c = 0;

        while (c++ < fstart) {
            b = string_utf8_skip_character(b);
        }

        int64_t astart = b - string->items;

        while (c++ <= fend) {
            b = string_utf8_skip_character(b);
        }

        int64_t aend = b - string->items;

        vm_push(vm, OBJ_VAL(vm_copy_string(vm, string->items + astart,
                                           aend - astart)));
    } else {
        vm_error(vm, "expected an array or a string, got %s",
                 value_description(target));

        return false;
    }

    return true;
}

static bool vm_make_slice_under(Vm *vm) {
    Value target = vm_pop(vm);

    Value end = vm_pop(vm);

    if (!IS_NUM(end)) {
        vm_error(vm, "cannot slice using %s value, expected a number",
                 value_description(end));

        return false;
    }

    double fend = AS_NUM(end);

    if (IS_ARRAY(target)) {
        ObjArray *array = AS_ARRAY(target);

        if (fend < 0) {
            fend += array->count;
        }

        if (fend < 0 || fend > array->count || (fend - floor(fend)) != 0) {
            vm_error(vm,
                     "sliced array has %d elements, the slice end must be an "
                     "integer less than that and greater than zero, but got %g",
                     array->count, fend);

            return false;
        }

        vm_push(vm, OBJ_VAL(vm_copy_array(vm, array->items, fend)));
    } else if (IS_STRING(target)) {
        ObjString *string = AS_STRING(target);

        int64_t characters_count = string_utf8_characters_count(
            string->items, string->items + string->count);

        if (fend < 0) {
            fend += characters_count;
        }

        if (fend < 0 || fend > characters_count || (fend - floor(fend)) != 0) {
            vm_error(
                vm,
                "sliced string has %ld characters, the slice end must be "
                "an integer less than that and greater than zero, but got %g",
                characters_count, fend);

            return false;
        }

        const char *b = string->items;

        int64_t c = 0;

        while (++c <= fend) {
            b = string_utf8_skip_character(b);
        }

        vm_push(vm,
                OBJ_VAL(vm_copy_string(vm, string->items, b - string->items)));
    } else {
        vm_error(vm, "expected an array or a string, got %s",
                 value_description(target));

        return false;
    }

    return true;
}

static bool vm_make_slice_above(Vm *vm) {
    Value target = vm_pop(vm);

    Value start = vm_pop(vm);

    if (!IS_NUM(start)) {
        vm_error(vm, "cannot slice using %s value, expected a number",
                 value_description(start));

        return false;
    }

    double fstart = AS_NUM(start);

    if (IS_ARRAY(target)) {
        ObjArray *array = AS_ARRAY(target);

        if (fstart < 0) {
            fstart += array->count;
        }

        if (fstart < 0 || fstart >= array->count ||
            (fstart - floor(fstart)) != 0) {
            vm_error(vm,
                     "sliced array has %d elements, the slice start must be an "
                     "integer less than that and greater than zero, but got %g",
                     array->count, fstart);

            return false;
        }

        vm_push(vm, OBJ_VAL(vm_copy_array(vm, array->items + (size_t)fstart,
                                          array->count - fstart)));
    } else if (IS_STRING(target)) {
        ObjString *string = AS_STRING(target);

        int64_t characters_count = string_utf8_characters_count(
            string->items, string->items + string->count);

        if (fstart < 0) {
            fstart += characters_count;
        }

        if (fstart < 0 || fstart >= characters_count ||
            (fstart - floor(fstart)) != 0) {
            vm_error(
                vm,
                "sliced string has %ld characters, the slice start must be "
                "an integer less than that and greater than zero, but got %g",
                characters_count, fstart);

            return false;
        }

        const char *b = string->items;

        int64_t c = 0;

        while (c++ < fstart) {
            b = string_utf8_skip_character(b);
        }

        vm_push(vm,
                OBJ_VAL(vm_copy_string(
                    vm, b, string->count - (uintptr_t)(b - string->items))));
    } else {
        vm_error(vm, "expected an array or a string, got %s",
                 value_description(target));

        return false;
    }

    return true;
}

static inline bool vm_execute_math(Vm *vm, OpCode op) {
    switch (op) {
    case OP_ADD: {
        if (!vm_add(vm)) {
            return false;
        }

        break;
    }

    case OP_SUB: {
        if (!vm_sub(vm)) {
            return false;
        }

        break;
    }

    case OP_MUL: {
        if (!vm_mul(vm)) {
            return false;
        }

        break;
    }

    case OP_DIV: {
        if (!vm_div(vm)) {
            return false;
        }

        break;
    }

    case OP_MOD: {
        if (!vm_mod(vm)) {
            return false;
        }

        break;
    }

    case OP_POW: {
        if (!vm_pow(vm)) {
            return false;
        }

        break;
    }

    default:
        assert(false && "UNREACHABLE");
    }

    return true;
}

bool vm_run(Vm *vm, Value *result) {
    CallFrame *frame = &vm->frames[vm->frame_count - 1];

#include "vm_readers.h"

#define vmfetch() op = READ_BYTE();

#ifdef NUR_NO_JUMPTABLE
#define vmdispatch() switch (op)
#define vmcase(op) case op:
#define vmbreak() break
#else
#include "vm_jumptable.h"
#endif

    OpCode op;

    for (;;) {
        vmfetch();

        vmdispatch() {
            vmcase(OP_POP) {
                vm_pop(vm);
                vmbreak();
            }

            vmcase(OP_DUP) {
                vm_push(vm, vm_peek(vm, 0));
                vmbreak();
            }

            vmcase(OP_SWP) {
                Value a = vm_peek(vm, 0);
                Value b = vm_peek(vm, 1);
                vm_poke(vm, 0, b);
                vm_poke(vm, 1, a);
                vmbreak();
            }

            vmcase(OP_PUSH_NULL) {
                vm_push(vm, NULL_VAL);
                vmbreak();
            }

            vmcase(OP_PUSH_TRUE) {
                vm_push(vm, BOOL_VAL(true));
                vmbreak();
            }

            vmcase(OP_PUSH_FALSE) {
                vm_push(vm, BOOL_VAL(false));
                vmbreak();
            }

            vmcase(OP_PUSH_CONST) {
                vm_push(vm, READ_CONSTANT());
                vmbreak();
            }

            vmcase(OP_GET_LOCAL) {
                vm_push(vm, frame->slots[READ_BYTE()]);
                vmbreak();
            }

            vmcase(OP_SET_LOCAL) {
                frame->slots[READ_BYTE()] = vm_peek(vm, 0);
                vmbreak();
            }

            vmcase(OP_SET_LOCAL_WITH_MATH) {
                OpCode op = READ_BYTE();
                uint8_t index = READ_BYTE();

                Value rhs = vm_pop(vm);
                Value lhs = frame->slots[index];

                vm_push(vm, lhs);
                vm_push(vm, rhs);

                vm_execute_math(vm, op);

                frame->slots[index] = vm_peek(vm, 0);
                vmbreak();
            }

            vmcase(OP_GET_UPVALUE) {
                vm_push(vm, *frame->closure->upvalues[READ_BYTE()]->location);
                vmbreak();
            }

            vmcase(OP_SET_UPVALUE) {
                *frame->closure->upvalues[READ_BYTE()]->location =
                    vm_peek(vm, 0);
                vmbreak();
            }

            vmcase(OP_SET_UPVALUE_WITH_MATH) {
                OpCode op = READ_BYTE();
                uint8_t index = READ_BYTE();

                Value rhs = vm_pop(vm);
                Value lhs = *frame->closure->upvalues[index]->location;

                vm_push(vm, lhs);
                vm_push(vm, rhs);

                vm_execute_math(vm, op);

                *frame->closure->upvalues[index]->location = vm_peek(vm, 0);

                vmbreak();
            }

            vmcase(OP_CLOSE_UPVALUE) {
                vm_close_upvalues(vm, vm->sp - 1);
                vm_pop(vm);
                vmbreak();
            }

            vmcase(OP_GET_GLOBAL) {
                ObjString *key = READ_STRING();

                Value value;

                if (!vm_map_lookup(frame->closure->fn->globals, key, &value)) {
                    vm_error(vm, "'%.*s' is not defined", (int)key->count,
                             key->items);

                    return false;
                }

                vm_push(vm, value);

                vmbreak();
            }

            vmcase(OP_SET_GLOBAL) {
                vm_map_insert(vm, frame->closure->fn->globals, READ_STRING(),
                              vm_peek(vm, 0));

                vmbreak();
            }

            vmcase(OP_SET_GLOBAL_WITH_MATH) {
                OpCode op = READ_BYTE();
                ObjString *key = READ_STRING();

                Value rhs = vm_pop(vm);
                Value lhs;

                if (!vm_map_lookup(frame->closure->fn->globals, key, &lhs)) {
                    vm_error(vm,
                             "'%.*s' is not "
                             "defined",
                             (int)key->count, key->items);

                    return false;
                }

                vm_push(vm, lhs);
                vm_push(vm, rhs);

                vm_execute_math(vm, op);

                vm_map_insert(vm, frame->closure->fn->globals, key,
                              vm_peek(vm, 0));

                vmbreak();
            }

            vmcase(OP_GET_SUBSCRIPT) {
                Value target = vm_pop(vm);
                Value index = vm_pop(vm);

                if (!vm_get_subscript(vm, target, index)) {
                    return false;
                }

                vmbreak();
            }

            vmcase(OP_SET_SUBSCRIPT) {
                Value target = vm_pop(vm);
                Value index = vm_pop(vm);

                if (!vm_set_subscript(vm, target, index)) {
                    return false;
                }

                vmbreak();
            }

            vmcase(OP_SET_SUBSCRIPT_WITH_MATH) {
                OpCode op = READ_BYTE();

                Value target = vm_pop(vm);
                Value index = vm_pop(vm);
                Value rhs = vm_pop(vm);

                if (!vm_get_subscript(vm, target, index)) {
                    return false;
                }

                vm_push(vm, rhs);

                if (!vm_execute_math(vm, op)) {
                    return false;
                }

                if (!vm_set_subscript(vm, target, index)) {
                    return false;
                }

                vmbreak();
            }

            vmcase(OP_MAKE_ARRAY) {
                uint32_t count = READ_WORD();

                ObjArray *array = vm_copy_array(vm, vm->sp - count, count);

                vm->sp -= count;

                vm_push(vm, OBJ_VAL(array));

                vmbreak();
            }

            vmcase(OP_COPY_BY_SLICING) {
                Value target = vm_pop(vm);

                if (IS_ARRAY(target)) {
                    ObjArray *array = AS_ARRAY(target);

                    vm_push(vm, OBJ_VAL(vm_copy_array(vm, array->items,
                                                      array->count)));
                } else if (IS_STRING(target)) {
                    ObjString *string = AS_STRING(target);

                    vm_push(vm, OBJ_VAL(vm_copy_string(vm, string->items,
                                                       string->count)));
                } else {
                    vm_error(vm, "%s is not an array value",
                             value_description(target));

                    return false;
                }

                vmbreak();
            }

            vmcase(OP_MAKE_SLICE) {
                if (!vm_make_slice(vm)) {
                    return false;
                }

                vmbreak();
            }

            vmcase(OP_MAKE_SLICE_UNDER) {
                if (!vm_make_slice_under(vm)) {
                    return false;
                }

                vmbreak();
            }

            vmcase(OP_MAKE_SLICE_ABOVE) {
                if (!vm_make_slice_above(vm)) {
                    return false;
                }

                vmbreak();
            }

            vmcase(OP_MAKE_MAP) {
                uint32_t count = READ_WORD();

                ObjMap *map = vm_new_map(vm);

                Value *start = vm->sp - count * 2;

                for (uint32_t i = 0; i < count * 2; i += 2) {
                    if (!IS_STRING(start[i])) {
                        vm_error(vm, "expected a string got %s",
                                 value_description(start[i]));

                        return false;
                    }

                    ObjString *key = AS_STRING(start[i]);
                    Value value = start[i + 1];

                    vm_map_insert(vm, map, key, value);
                }

                vm->sp = start;

                vm_push(vm, OBJ_VAL(map));

                vmbreak();
            }

            vmcase(OP_MAKE_CLOSURE) {
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

                vmbreak();
            }

            vmcase(OP_EQL) {
                vm_push(vm, BOOL_VAL(values_equal(vm_pop(vm), vm_pop(vm))));
                vmbreak();
            }

            vmcase(OP_NEQ) {
                vm_push(vm, BOOL_VAL(!values_equal(vm_pop(vm), vm_pop(vm))));
                vmbreak();
            }

            vmcase(OP_NOT) {
                vm_poke(vm, 0, BOOL_VAL(value_is_falsey(vm_peek(vm, 0))));
                vmbreak();
            }

            vmcase(OP_NEG) {
                if (!vm_neg(vm)) {
                    return false;
                }

                vmbreak();
            }

            vmcase(OP_ADD) {
                if (!vm_add(vm)) {
                    return false;
                }

                vmbreak();
            }

            vmcase(OP_SUB) {
                if (!vm_sub(vm)) {
                    return false;
                }

                vmbreak();
            }

            vmcase(OP_MUL) {
                if (!vm_mul(vm)) {
                    return false;
                }

                vmbreak();
            }

            vmcase(OP_DIV) {
                if (!vm_div(vm)) {
                    return false;
                }

                vmbreak();
            }

            vmcase(OP_MOD) {
                if (!vm_mod(vm)) {
                    return false;
                }

                vmbreak();
            }

            vmcase(OP_POW) {
                if (!vm_pow(vm)) {
                    return false;
                }

                vmbreak();
            }

            vmcase(OP_LT) {
                if (!vm_lt(vm)) {
                    return false;
                }

                vmbreak();
            }

            vmcase(OP_GT) {
                if (!vm_gt(vm)) {
                    return false;
                }

                vmbreak();
            }

            vmcase(OP_LTE) {
                if (!vm_lte(vm)) {
                    return false;
                }

                vmbreak();
            }

            vmcase(OP_GTE) {
                if (!vm_gte(vm)) {
                    return false;
                }

                vmbreak();
            }

            vmcase(OP_CALL) {
                if (!vm_call_value(vm, vm_pop(vm), READ_BYTE())) {
                    return false;
                }

                frame = &vm->frames[vm->frame_count - 1];

                vmbreak();
            }

            vmcase(OP_POP_JUMP_IF_FALSE) {
                uint16_t offset = READ_SHORT();

                if (value_is_falsey(vm_pop(vm)))
                    frame->ip += offset;

                vmbreak();
            }

            vmcase(OP_JUMP_IF_FALSE) {
                uint16_t offset = READ_SHORT();

                if (value_is_falsey(vm_peek(vm, 0)))
                    frame->ip += offset;

                vmbreak();
            }

            vmcase(OP_JUMP) {
                uint16_t offset = READ_SHORT();

                frame->ip += offset;

                vmbreak();
            }

            vmcase(OP_LOOP) {
                uint16_t offset = READ_SHORT();

                frame->ip -= offset;

                vmbreak();
            }

            vmcase(OP_RETURN) {
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

                vmbreak();
            }
        }
    }
}
