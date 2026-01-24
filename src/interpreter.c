#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "parser.h"
#include "source_location.h"
#include "vm.h"

typedef struct {
    const char *file_path;
    const char *file_buffer;
    Vm *vm;
    Chunk *chunk;
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

static bool compile_stmt(Compiler *compiler, AstNodeIdx node_idx);
static bool compile_expr(Compiler *compiler, AstNodeIdx node_idx);

static bool compile_block(Compiler *compiler, AstNode block) {
    for (uint32_t i = 0; i < block.rhs; i++) {
        if (!compile_stmt(compiler, compiler->ast.extra.items[block.lhs + i])) {
            return false;
        }
    }

    return true;
}

static bool compile_return(Compiler *compiler, AstNode node, uint32_t source) {
    if (!compile_expr(compiler, node.rhs)) {
        return false;
    }

    chunk_add_byte(compiler->chunk, OP_RETURN, source);

    compiler->returned = true;

    return true;
}

static void compiler_push_constant(Compiler *compiler, Value value,
                                   uint32_t source) {
    uint16_t c = chunk_add_constant(compiler->chunk, value);
    chunk_add_byte(compiler->chunk, OP_CONST, source);
    chunk_add_byte(compiler->chunk, c >> 8, source);
    chunk_add_byte(compiler->chunk, c, source);
}

static void compile_int(Compiler *compiler, AstNode node, uint32_t source) {
    uint64_t v = (uint64_t)node.lhs << 32 | node.rhs;
    compiler_push_constant(compiler, INT_VAL(v), source);
}

static void compile_float(Compiler *compiler, AstNode node, uint32_t source) {
    uint64_t v = (uint64_t)node.lhs << 32 | node.rhs;
    double fv;
    memcpy(&fv, &v, sizeof(double));
    compiler_push_constant(compiler, FLT_VAL(fv), source);
}

static bool compile_unary(Compiler *compiler, AstNode node, uint32_t source,
                          OpCode opcode) {
    if (!compile_expr(compiler, node.rhs)) {
        return false;
    }

    chunk_add_byte(compiler->chunk, opcode, source);

    return true;
}

static bool compile_binary(Compiler *compiler, AstNode node, uint32_t source,
                           OpCode opcode) {
    if (!compile_expr(compiler, node.lhs)) {
        return false;
    }

    if (!compile_expr(compiler, node.rhs)) {
        return false;
    }

    chunk_add_byte(compiler->chunk, opcode, source);

    return true;
}

static bool compile_stmt(Compiler *compiler, AstNodeIdx node_idx) {
    AstNode node = compiler->ast.nodes.items[node_idx];
    uint32_t source = compiler->ast.nodes.sources[node_idx];

    switch (node.tag) {
    case NODE_BLOCK:
        return compile_block(compiler, node);

    case NODE_RETURN:
        return compile_return(compiler, node, source);

    default:
        if (!compile_expr(compiler, node_idx)) {
            return false;
        }

        chunk_add_byte(compiler->chunk, OP_POP, source);

        return true;
    }
}

static bool compile_expr(Compiler *compiler, AstNodeIdx node_idx) {
    AstNode node = compiler->ast.nodes.items[node_idx];
    uint32_t source = compiler->ast.nodes.sources[node_idx];

    switch (node.tag) {
    case NODE_INT:
        compile_int(compiler, node, source);
        return true;

    case NODE_FLOAT:
        compile_float(compiler, node, source);
        return true;

    case NODE_NOT:
        return compile_unary(compiler, node, source, OP_NOT);

    case NODE_NEG:
        return compile_unary(compiler, node, source, OP_NEG);

    case NODE_ADD:
        return compile_binary(compiler, node, source, OP_ADD);

    case NODE_SUB:
        return compile_binary(compiler, node, source, OP_SUB);

    case NODE_MUL:
        return compile_binary(compiler, node, source, OP_MUL);

    case NODE_DIV:
        return compile_binary(compiler, node, source, OP_DIV);

    case NODE_POW:
        return compile_binary(compiler, node, source, OP_POW);

    case NODE_MOD:
        return compile_binary(compiler, node, source, OP_MOD);

    case NODE_EQL:
        return compile_binary(compiler, node, source, OP_EQL);

    case NODE_NEQ:
        return compile_binary(compiler, node, source, OP_NEQ);

    case NODE_LT:
        return compile_binary(compiler, node, source, OP_LT);

    case NODE_GT:
        return compile_binary(compiler, node, source, OP_GT);

    case NODE_LTE:
        return compile_binary(compiler, node, source, OP_LTE);

    case NODE_GTE:
        return compile_binary(compiler, node, source, OP_GTE);

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

    CallFrame *frame = &vm.frames[vm.frame_count++];

    frame->fn = vm_new_function(&vm,
                                (Chunk){
                                    .file_path = file_path,
                                    .file_content = file_buffer,
                                },
                                0);

    frame->slots = vm.stack;

    Compiler compiler = {
        .file_path = file_path,
        .file_buffer = file_buffer,
        .ast = parser.ast,
        .chunk = &frame->fn->chunk,
    };

    if (!compile_block(&compiler, block)) {
        return false;
    }

    free(parser.ast.nodes.items);
    free(parser.ast.nodes.sources);
    free(parser.ast.extra.items);
    free(parser.ast.strings.items);

    if (!compiler.returned) {
        chunk_add_byte(compiler.chunk, OP_NULL, 0);
        chunk_add_byte(compiler.chunk, OP_RETURN, 0);
    }

    frame->ip = frame->fn->chunk.bytes;

    Value result;

    if (!vm_run(&vm, &result)) {
        return false;
    }

    vm_gc(&vm);

    return true;
}
