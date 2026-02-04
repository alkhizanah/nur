#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "array.h"
#include "vm.h"

void vm_insert_global(Vm *vm, const char *key, Value value) {
    vm_map_insert(vm, vm->globals, vm_copy_string(vm, key, strlen(key)), value);
}

void vm_insert_global_native(Vm *vm, const char *key, NativeFn call) {
    ObjNative *native = OBJ_ALLOC(vm, OBJ_NATIVE, ObjNative);
    native->fn = call;
    vm_insert_global(vm, key, OBJ_VAL(native));
}

bool vm_print(Vm *vm, Value *argv, uint8_t argc, Value *result) {
    (void)vm;

    for (uint8_t i = 0; i < argc; i++) {
        Value arg = argv[i];

        if (IS_STRING(arg)) {
            printf("%.*s", AS_STRING(arg)->count, AS_STRING(arg)->items);
        } else {
            value_display(arg);
        }
    }

    *result = NULL_VAL;

    return true;
}

bool vm_println(Vm *vm, Value *argv, uint8_t argc, Value *result) {
    vm_print(vm, argv, argc, result);

    printf("\n");

    return true;
}

bool vm_len(Vm *vm, Value *argv, uint8_t argc, Value *result) {
    if (argc != 1) {
        vm_error(vm, "len() takes exactly one argument, but got %d", argc);

        return false;
    }

    Value arg = argv[0];

    if (!IS_OBJ(arg)) {
        vm_error(vm, "can not get the length of %s value",
                 value_description(arg));

        return false;
    }

    switch (AS_OBJ(arg)->tag) {
    case OBJ_ARRAY:
        *result = INT_VAL(AS_ARRAY(arg)->count);

        return true;

    case OBJ_STRING:
        *result = INT_VAL(string_utf8_characters_count(
            AS_STRING(arg)->items,
            AS_STRING(arg)->items + AS_STRING(arg)->count));

        return true;

    case OBJ_MAP:
        *result = INT_VAL(AS_MAP(arg)->count);

        return true;

    default:
        vm_error(vm, "can not get the length of %s value",
                 value_description(arg));

        return false;
    }
}

bool vm_random(Vm *vm, Value *argv, uint8_t argc, Value *result) {
    if (argc != 2) {
        vm_error(vm, "random() takes exactly two arguments, but got %d", argc);

        return false;
    }

    Value first = argv[0];
    Value second = argv[1];

    if (!IS_INT(first) && !IS_FLT(first)) {
        vm_error(vm,
                 "error: expected first argument to be a number, got %s value",
                 value_description(first));

        return false;
    }

    if (!IS_INT(second) && !IS_FLT(second)) {
        vm_error(vm,
                 "error: expected second argument to be a number, got %s value",
                 value_description(second));

        return false;
    }

    double min = value_to_float(first);
    double max = value_to_float(second);

    if (min > max) {
        double temp = max;
        max = min;
        min = temp;
    }

    double r = (double)rand() / ((double)RAND_MAX + 1.0);
    double v = min + r * (max - min);

    *result = FLT_VAL(v);

    return true;
}

bool vm_array_push(Vm *vm, Value *argv, uint8_t argc, Value *result) {
    if (argc != 2) {
        vm_error(vm, "array_push() takes exactly two arguments, but got %d",
                 argc);

        return false;
    }

    if (!IS_ARRAY(argv[0])) {
        vm_error(
            vm,
            "array_push() requires first argument to be an array, but got %s",
            value_description(argv[0]));

        return false;
    }

    ObjArray *array = AS_ARRAY(argv[0]);

    Value value = argv[1];

    uint32_t old_capacity = array->capacity;

    ARRAY_PUSH(array, value);

    vm->bytes_allocated += (array->capacity - old_capacity) * sizeof(Value);

    if (vm->bytes_allocated > vm->next_gc) {
        vm_gc(vm);
    }

    *result = NULL_VAL;

    return true;
}

bool vm_array_pop(Vm *vm, Value *argv, uint8_t argc, Value *result) {
    if (argc != 1) {
        vm_error(vm, "array_pop() takes exactly one argument, but got %d",
                 argc);

        return false;
    }

    if (!IS_ARRAY(argv[0])) {
        vm_error(
            vm,
            "array_pop() requires first argument to be an array, but got %s",
            value_description(argv[0]));

        return false;
    }

    ObjArray *array = AS_ARRAY(argv[0]);

    *result = array->items[--array->count];

    return true;
}

bool vm_to_int(Vm *vm, Value *argv, uint8_t argc, Value *result) {
    if (argc != 1) {
        vm_error(vm, "to_int() takes exactly one argument, but got %d", argc);

        return false;
    }

    Value value = argv[0];

    if (IS_INT(value)) {
        *result = value;
    } else if (IS_STRING(value)) {
        ObjString *string = AS_STRING(value);

        char *endptr;

        errno = 0;

        int64_t v = strtoll(string->items, &endptr, 0);

        if (errno == ERANGE || endptr != string->items + string->count) {
            *result = NULL_VAL;

            return true;
        }

        *result = INT_VAL(v);
    } else if (IS_FLT(value)) {
        *result = INT_VAL(AS_FLT(value));
    } else if (IS_BOOL(value)) {
        *result = INT_VAL(AS_BOOL(value));
    } else {
        *result = NULL_VAL;
    }

    return true;
}

void vm_insert_globals(Vm *vm) {
    srand(time(NULL));

    vm_insert_global_native(vm, "print", vm_print);
    vm_insert_global_native(vm, "println", vm_println);
    vm_insert_global_native(vm, "len", vm_len);
    vm_insert_global_native(vm, "random", vm_random);
    vm_insert_global_native(vm, "array_push", vm_array_push);
    vm_insert_global_native(vm, "array_pop", vm_array_pop);
    vm_insert_global_native(vm, "to_int", vm_to_int);
}
