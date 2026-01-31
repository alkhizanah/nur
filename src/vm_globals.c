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

bool vm_print(Vm *vm, Value *argv, uint8_t argc) {
    for (uint8_t i = 0; i < argc; i++) {
        Value arg = argv[i];

        if (IS_STRING(arg)) {
            printf("%.*s", AS_STRING(arg)->count, AS_STRING(arg)->items);
        } else {
            value_display(arg);
        }
    }

    vm_push(vm, NULL_VAL);

    return true;
}

bool vm_println(Vm *vm, Value *argv, uint8_t argc) {
    vm_print(vm, argv, argc);
    printf("\n");
    return true;
}

void vm_insert_globals(Vm *vm) {
    vm_insert_global_native(vm, "print", vm_print);
    vm_insert_global_native(vm, "println", vm_println);
}
