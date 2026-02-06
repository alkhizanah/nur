#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef WINDOWS
#include <direct.h>
#define get_current_working_directory _getcwd
#else
#include <unistd.h>
#define get_current_working_directory getcwd
#endif

#include "array.h"
#include "fs.h"
#include "vm.h"

bool vm_map_insert_by_cstr(Vm *vm, ObjMap *map, const char *key, Value value) {
    return vm_map_insert(vm, map, vm_copy_string(vm, key, strlen(key)), value);
}

bool vm_map_insert_native_by_cstr(Vm *vm, ObjMap *map, const char *key,
                                  NativeFn fn) {
    ObjNative *native = OBJ_ALLOC(vm, OBJ_NATIVE, ObjNative);
    native->fn = fn;
    return vm_map_insert_by_cstr(vm, map, key, OBJ_VAL(native));
}

bool vm_builtin_print(Vm *vm, Value *argv, uint8_t argc, Value *result) {
    (void)vm;

    for (uint8_t i = 0; i < argc; i++) {
        Value arg = argv[i];

        if (i > 0) {
            printf(" ");
        }

        if (IS_STRING(arg)) {
            printf("%.*s", AS_STRING(arg)->count, AS_STRING(arg)->items);
        } else {
            value_display(arg);
        }
    }

    *result = NULL_VAL;

    return true;
}

bool vm_builtin_println(Vm *vm, Value *argv, uint8_t argc, Value *result) {
    vm_builtin_print(vm, argv, argc, result);

    printf("\n");

    return true;
}

bool vm_builtin_len(Vm *vm, Value *argv, uint8_t argc, Value *result) {
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

bool vm_builtin_random(Vm *vm, Value *argv, uint8_t argc, Value *result) {
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

bool vm_builtin_array_push(Vm *vm, Value *argv, uint8_t argc, Value *result) {
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

bool vm_builtin_array_pop(Vm *vm, Value *argv, uint8_t argc, Value *result) {
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

bool vm_builtin_to_int(Vm *vm, Value *argv, uint8_t argc, Value *result) {
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

static ObjMap *modules = NULL;

bool vm_builtin_import(Vm *vm, Value *argv, uint8_t argc, Value *result) {
    if (argc != 1) {
        vm_error(vm, "import() takes exactly one argument, but got %d", argc);

        return false;
    }

    if (!IS_STRING(argv[0])) {
        vm_error(vm, "import() takes a string as an argument, but got %s",
                 value_description(argv[0]));

        return false;
    }

    ObjString *original_name = AS_STRING(argv[0]);

    if (vm_map_lookup(modules, original_name, result)) {
        return true;
    }

    const char *parent_file_path =
        vm->frames[vm->frame_count - 1].closure->fn->chunk.file_path;

    ObjString *parent_directory =
        vm_copy_string(vm, parent_file_path, strlen(parent_file_path));

    while (parent_directory->items[parent_directory->count - 1] != '/'
#ifdef _WIN32
           && parent_directory->items[parent_directory->count - 1] != '\\'
#endif
    ) {
        parent_directory->count--;
    }

    ObjString *new_name =
        vm_concat_strings(vm, parent_directory, original_name);

    new_name->items =
        realloc(new_name->items, (new_name->count + 1) * sizeof(char));

    if (new_name->items == NULL) {
        fprintf(stderr, "error: out of memory\n");

        exit(1);
    }

    new_name->items[new_name->count] = '\0'; // Adding null termination

    if (file_exists(new_name->items)) {
        char *file_content = read_entire_file(new_name->items);

        Vm mvm = {0};

        vm_init(&mvm);

        if (!vm_load_file(&mvm, new_name->items, file_content)) {
            return false;
        }

        if (!vm_run(&mvm, result)) {
            return false;
        }

        vm_map_insert(vm, modules, new_name, *result);

        return true;
    }

    vm_error(vm, "module '%.*s' is not found, even if we use '%.*s' instead",
             (int)original_name->count, original_name->items,
             (int)new_name->count, new_name->items);

    return false;
}

void vm_map_insert_builtins(Vm *vm, ObjMap *map) {
    srand(time(NULL));

    if (modules == NULL) {
        modules = vm_new_map(vm);
    }

    vm_map_insert_by_cstr(vm, map, "__modules__", OBJ_VAL(modules));

    vm_map_insert_native_by_cstr(vm, map, "print", vm_builtin_print);
    vm_map_insert_native_by_cstr(vm, map, "println", vm_builtin_println);
    vm_map_insert_native_by_cstr(vm, map, "len", vm_builtin_len);
    vm_map_insert_native_by_cstr(vm, map, "random", vm_builtin_random);
    vm_map_insert_native_by_cstr(vm, map, "array_push", vm_builtin_array_push);
    vm_map_insert_native_by_cstr(vm, map, "array_pop", vm_builtin_array_pop);
    vm_map_insert_native_by_cstr(vm, map, "to_int", vm_builtin_to_int);
    vm_map_insert_native_by_cstr(vm, map, "import", vm_builtin_import);
}
