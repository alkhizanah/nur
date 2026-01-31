#include <errno.h>
#include <malloc.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "array.h"
#include "ast.h"
#include "compiler.h"
#include "parser.h"
#include "vm.h"

int file_exists(const char *file_path) {
#if _WIN32
#include <windows.h>
    return GetFileAttributesA(file_path) != INVALID_FILE_ATTRIBUTES;
#else
#include <unistd.h>
    return access(file_path, F_OK) == 0;
#endif
}

char *read_entire_file(const char *file_path) {
    FILE *file = fopen(file_path, "r");

    if (file == NULL) {
        fprintf(stderr, "error: could not open file '%s': %s\n", file_path,
                strerror(errno));

        exit(1);
    }

    char c;

    fread(&c, 1, 1, file);

    if (ferror(file)) {
        fprintf(stderr, "error: could not read file '%s': %s\n", file_path,
                strerror(errno));

        fclose(file);

        exit(1);
    }

    if (fseek(file, 0, SEEK_END) == -1) {
        fprintf(stderr, "error: could not get size of file '%s': %s\n",
                file_path, strerror(errno));

        fclose(file);

        exit(1);
    }

    long file_size = ftell(file);

    if (file_size == -1) {
        fprintf(stderr, "error: could not get size of file '%s': %s\n",
                file_path, strerror(errno));

        fclose(file);

        exit(1);
    }

    rewind(file);

    char *buffer = malloc(sizeof(char) * (file_size + 1));

    if (buffer == NULL) {
        fprintf(stderr, "error: out of memory\n");

        fclose(file);

        exit(1);
    }

    fread(buffer, 1, file_size, file);

    if (ferror(file)) {
        fprintf(stderr, "error: could not read file '%s': %s\n", file_path,
                strerror(errno));

        fclose(file);

        exit(1);
    }

    buffer[file_size] = 0;

    return buffer;
}

static void disassemble(Chunk chunk) {
    size_t ip = 0;

    for (; ip < chunk.count; printf("\n")) {
        printf("%04zu\t", ip);

        OpCode opcode = chunk.bytes[ip++];

        switch (opcode) {
        case OP_POP:
            printf("POP");
            break;

        case OP_DUP:
            printf("DUP");
            break;

        case OP_SWP:
            printf("SWP");
            break;

        case OP_PUSH_NULL:
            printf("PUSH_NULL");
            break;

        case OP_PUSH_TRUE:
            printf("PUSH_TRUE");
            break;

        case OP_PUSH_FALSE:
            printf("PUSH_FALSE");
            break;

        case OP_PUSH_CONST: {
            uint16_t index = (ip += 2, ((uint16_t)chunk.bytes[ip - 2] << 8) |
                                           chunk.bytes[ip - 1]);

            printf("PUSH_CONST %d (", (int)index);

            value_display(chunk.constants.items[index]);

            printf(")");

            break;
        }

        case OP_MAKE_CLOSURE: {
            uint16_t index = (ip += 2, ((uint16_t)chunk.bytes[ip - 2] << 8) |
                                           chunk.bytes[ip - 1]);

            printf("MAKE_CLOSURE %d (<function>)", (int)index);

            ObjFunction *function = AS_FUNCTION(chunk.constants.items[index]);

            for (int i = 0; i < function->upvalues_count; i++) {
                int isLocal = chunk.bytes[ip++];
                int index = chunk.bytes[ip++];

                printf("\n%04zu\t|\t\t%s %d", ip - 2,
                       isLocal ? "local" : "upvalue", index);
            }

            break;
        }

        case OP_GET_LOCAL:
            printf("GET_LOCAL %d", (int)chunk.bytes[ip++]);
            break;

        case OP_SET_LOCAL:
            printf("SET_LOCAL %d", (int)chunk.bytes[ip++]);
            break;

        case OP_GET_UPVALUE:
            printf("GET_UPVALUE %d", (int)chunk.bytes[ip++]);
            break;

        case OP_SET_UPVALUE:
            printf("SET_UPVALUE %d", (int)chunk.bytes[ip++]);
            break;

        case OP_CLOSE_UPVALUE:
            printf("CLOSE_UPVALUE");
            break;

        case OP_GET_GLOBAL: {
            uint16_t index = (ip += 2, ((uint16_t)chunk.bytes[ip - 2] << 8) |
                                           chunk.bytes[ip - 1]);

            printf("GET_GLOBAL ");

            value_display(chunk.constants.items[index]);

            break;
        }

        case OP_SET_GLOBAL: {
            uint16_t index = (ip += 2, ((uint16_t)chunk.bytes[ip - 2] << 8) |
                                           chunk.bytes[ip - 1]);

            printf("SET_GLOBAL ");

            value_display(chunk.constants.items[index]);

            break;
        }

        case OP_GET_SUBSCRIPT:
            printf("GET_SUBSCRIPT");
            break;

        case OP_SET_SUBSCRIPT:
            printf("SET_SUBSCRIPT");
            break;

        case OP_EQL:
            printf("EQL");
            break;

        case OP_NEQ:
            printf("NEQ");
            break;

        case OP_NOT:
            printf("NOT");
            break;

        case OP_NEG:
            printf("NEG");
            break;

        case OP_ADD:
            printf("ADD");
            break;

        case OP_SUB:
            printf("SUB");
            break;

        case OP_MUL:
            printf("MUL");
            break;

        case OP_DIV:
            printf("DIV");
            break;

        case OP_MOD:
            printf("MOD");
            break;

        case OP_POW:
            printf("POW");

            break;

        case OP_LT:
            printf("LT");
            break;

        case OP_GT:
            printf("GT");
            break;

        case OP_LTE:
            printf("LTE");
            break;

        case OP_GTE:
            printf("GTE");
            break;

        case OP_MAKE_ARRAY: {
            uint32_t count =
                (ip += 4, ((uint16_t)chunk.bytes[ip - 4] << 24) |
                              ((uint16_t)chunk.bytes[ip - 3] << 16) |
                              ((uint16_t)chunk.bytes[ip - 2] << 8) |
                              chunk.bytes[ip - 1]);

            printf("MAKE_ARRAY %d", (int)count);

            break;
        }

        case OP_MAKE_MAP: {
            uint32_t count =
                (ip += 4, ((uint16_t)chunk.bytes[ip - 4] << 24) |
                              ((uint16_t)chunk.bytes[ip - 3] << 16) |
                              ((uint16_t)chunk.bytes[ip - 2] << 8) |
                              chunk.bytes[ip - 1]);

            printf("MAKE_MAP %d", (int)count);

            break;
        }

        case OP_CALL:
            printf("CALL %d", (int)chunk.bytes[ip++]);
            break;

        case OP_POP_JUMP_IF_FALSE: {
            uint16_t offset = (ip += 2, ((uint16_t)chunk.bytes[ip - 2] << 8) |
                                            chunk.bytes[ip - 1]);

            printf("POP_JUMP_IF_FALSE TO %zu", ip + offset);

            break;
        }

        case OP_JUMP_IF_FALSE: {
            uint16_t offset = (ip += 2, ((uint16_t)chunk.bytes[ip - 2] << 8) |
                                            chunk.bytes[ip - 1]);

            printf("JUMP_IF_FALSE TO %zu", ip + offset);

            break;
        }

        case OP_JUMP: {
            uint16_t offset = (ip += 2, ((uint16_t)chunk.bytes[ip - 2] << 8) |
                                            chunk.bytes[ip - 1]);

            printf("JUMP TO %zu", ip + offset);

            break;
        }

        case OP_LOOP: {
            uint16_t offset = (ip += 2, ((uint16_t)chunk.bytes[ip - 2] << 8) |
                                            chunk.bytes[ip - 1]);

            printf("LOOP TO %zu", ip - offset);

            break;
        }

        case OP_RETURN:
            printf("RETURN");
            break;
        }
    }

    for (size_t i = 0; i < chunk.constants.count; i++) {
        Value constant = chunk.constants.items[i];

        if (IS_FUNCTION(constant)) {
            printf("FUNCTION (%zu):\n", i);
            disassemble(AS_FUNCTION(constant)->chunk);
        }
    }
}

static void usage(const char *program) {
    fprintf(stderr, "usage: %s <command> [options..] [arguments..]\n\n",
            program);
    fprintf(stderr, "commands:\n");
    fprintf(stderr, "\trun <input_file_path> - execute the specified file\n");
    fprintf(stderr, "\tdis <input_file_path> - compile and disassemble the "
                    "specified file\n");
    fprintf(stderr, "\n");
}

int main(int argc, const char **argv) {
    const char *program = ARRAY_SHIFT(argc, argv);

    if (argc == 0) {
        usage(program);
        fprintf(stderr, "error: command was not provided\n");

        return 1;
    }

    const char *command = ARRAY_SHIFT(argc, argv);

    Vm vm = {0};

    vm_init(&vm);

    if (strcmp(command, "run") == 0) {
        if (argc == 0) {
            usage(program);
            fprintf(stderr, "error: input file path was not provided\n");

            return 1;
        }

        const char *input_file_path = ARRAY_SHIFT(argc, argv);

        char *input_file_content = read_entire_file(input_file_path);

        if (!vm_load_file(&vm, input_file_path, input_file_content)) {
            return 1;
        }

        Value result;

        if (!vm_run(&vm, &result)) {
            return 1;
        }

        free(input_file_content);
    } else if (strcmp(command, "dis") == 0) {
        if (argc == 0) {
            usage(program);
            fprintf(stderr, "error: input file path was not provided\n");

            return 1;
        }

        const char *input_file_path = ARRAY_SHIFT(argc, argv);

        char *input_file_content = read_entire_file(input_file_path);

        if (!vm_load_file(&vm, input_file_path, input_file_content)) {
            return 1;
        }

        disassemble(vm.frames[0].closure->fn->chunk);

        free(input_file_content);
    } else if (file_exists(command)) {
        const char *input_file_path = command;

        char *input_file_content = read_entire_file(input_file_path);

        if (!vm_load_file(&vm, input_file_path, input_file_content)) {
            return 1;
        }

        Value result;

        if (!vm_run(&vm, &result)) {
            return 1;
        }

        free(input_file_content);
    } else {
        usage(program);
        fprintf(stderr, "error: unknown command: %s\n", command);

        return 1;
    }
}
