#include <stdio.h>
#include <string.h>

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
        *result = INT_VAL(AS_STRING(arg)->count);

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

void vm_insert_globals(Vm *vm) {
    vm_insert_global_native(vm, "print", vm_print);
    vm_insert_global_native(vm, "println", vm_println);
    vm_insert_global_native(vm, "len", vm_len);
}
