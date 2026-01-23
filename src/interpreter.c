#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "ast.h"
#include "parser.h"
#include "source_location.h"
#include "vm.h"

typedef struct {
    const char *file_path;
    const char *file_buffer;
    Vm *vm;
    Chunk chunk;
    Ast ast;
    bool returned;
} Compiler;

[[gnu::format(printf, 3, 4)]]
static void compiler_error(const Compiler *compiler, uint32_t start,
                           const char *format, ...) {
    va_list args;

    va_start(args, format);

    SourceLocation loc =
        source_location_of(compiler->file_path, compiler->file_buffer, start);

    fprintf(stderr, "%s:%u:%u: error: ", loc.file_path, loc.line, loc.column);
    vfprintf(stderr, format, args);

    va_end(args);
}

static bool compile_node(Compiler *compiler, AstNodeIdx node_idx);

static bool compile_block(Compiler *compiler, AstNode block) {
    for (uint32_t i = 0; i < block.rhs; i++) {
        if (!compile_node(compiler, compiler->ast.extra.items[block.lhs + i])) {
            return false;
        }
    }

    return true;
}

static bool compile_return(Compiler *compiler, AstNode node, uint32_t source) {
    if (!compile_node(compiler, node.rhs)) {
        return false;
    }

    chunk_add_byte(&compiler->chunk, OP_RETURN, source);

    compiler->returned = true;

    return true;
}

static bool compile_node(Compiler *compiler, AstNodeIdx node_idx) {
    AstNode node = compiler->ast.nodes.items[node_idx];
    uint32_t source = compiler->ast.nodes.sources[node_idx];

    switch (node.tag) {
    case NODE_BLOCK:
        return compile_block(compiler, node);

    case NODE_RETURN:
        return compile_return(compiler, node, source);

    default:
        compiler_error(compiler, source, "todo: compile this\n");

        return false;
    }
}

bool interpret(const char *file_path, const char *file_buffer) {
    Parser parser = {.file_path = file_path, .lexer = {.buffer = file_buffer}};

    AstNodeIdx program = parse(&parser);

    if (program == INVALID_NODE_IDX) {
        return false;
    }

    AstNode block = parser.ast.nodes.items[program];

    Vm vm = {0};

    vm_init(&vm);

    Compiler compiler = {
        .file_path = file_path,
        .file_buffer = file_buffer,
        .ast = parser.ast,
        .chunk = {0},
    };

    if (!compile_block(&compiler, block)) {
        return false;
    }

    free(parser.ast.nodes.items);
    free(parser.ast.nodes.sources);
    free(parser.ast.extra.items);
    free(parser.ast.strings.items);

    if (!compiler.returned) {
        chunk_add_byte(&compiler.chunk, OP_NULL, 0);
        chunk_add_byte(&compiler.chunk, OP_RETURN, 0);
    }

    vm.frames[vm.frame_count++] = (CallFrame){
        .fn = vm_new_function(&vm, compiler.chunk, 0),
        .ip = compiler.chunk.bytes,
        .slots = vm.stack,
    };

    Value result;

    if (!vm_run(&vm, &result)) {
        return false;
    }

    vm_gc(&vm);

    return true;
}
