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
        printf("%-10zu", ip);

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

        case OP_GET_LOCAL:
            printf("GET_LOCAL %d", (int)chunk.bytes[ip++]);
            break;

        case OP_SET_LOCAL:
            printf("SET_LOCAL %d", (int)chunk.bytes[ip++]);
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

    if (strcmp(command, "run") == 0) {
        if (argc == 0) {
            usage(program);
            fprintf(stderr, "error: input file path was not provided\n");

            return 1;
        }

        const char *input_file_path = ARRAY_SHIFT(argc, argv);

        char *input_file_content = read_entire_file(input_file_path);

        Vm vm = {0};

        vm_init(&vm);

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

        Vm vm = {0};

        vm_init(&vm);

        if (!vm_load_file(&vm, input_file_path, input_file_content)) {
            return 1;
        }

        disassemble(vm.frames[0].fn->chunk);

        free(input_file_content);
    } else {
        usage(program);
        fprintf(stderr, "error: unknown command: %s\n", command);

        return 1;
    }
}
