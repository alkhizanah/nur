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

    chunk_add_byte(compiler->chunk, OP_RETURN, source);

    compiler->returned = true;

    return true;
}

static bool compile_int(Compiler *compiler, AstNode node, uint32_t source) {
    uint64_t v = (uint64_t)node.lhs << 32 | node.rhs;
    uint16_t c = chunk_add_constant(compiler->chunk, INT_VAL(v));
    chunk_add_byte(compiler->chunk, OP_CONST, source);
    chunk_add_byte(compiler->chunk, c >> 8, source);
    chunk_add_byte(compiler->chunk, c, source);
    return true;
}

#define COMPILER_UNOP_FN(name, opcode)                                         \
    static bool name(Compiler *compiler, AstNode node, uint32_t source) {      \
        if (!compile_node(compiler, node.rhs)) {                               \
            return false;                                                      \
        }                                                                      \
                                                                               \
        chunk_add_byte(compiler->chunk, opcode, source);                       \
                                                                               \
        return true;                                                           \
    }

COMPILER_UNOP_FN(compile_not, OP_NOT)
COMPILER_UNOP_FN(compile_neg, OP_NEG)

#define COMPILER_BINOP_FN(name, opcode)                                        \
    static bool name(Compiler *compiler, AstNode node, uint32_t source) {      \
        if (!compile_node(compiler, node.lhs)) {                               \
            return false;                                                      \
        }                                                                      \
                                                                               \
        if (!compile_node(compiler, node.rhs)) {                               \
            return false;                                                      \
        }                                                                      \
                                                                               \
        chunk_add_byte(compiler->chunk, opcode, source);                       \
                                                                               \
        return true;                                                           \
    }

COMPILER_BINOP_FN(compile_add, OP_ADD)
COMPILER_BINOP_FN(compile_sub, OP_SUB)
COMPILER_BINOP_FN(compile_mul, OP_MUL)
COMPILER_BINOP_FN(compile_div, OP_DIV)
COMPILER_BINOP_FN(compile_pow, OP_POW)

static bool compile_node(Compiler *compiler, AstNodeIdx node_idx) {
    AstNode node = compiler->ast.nodes.items[node_idx];
    uint32_t source = compiler->ast.nodes.sources[node_idx];

    switch (node.tag) {
    case NODE_BLOCK:
        return compile_block(compiler, node);

    case NODE_RETURN:
        return compile_return(compiler, node, source);

    case NODE_INT:
        return compile_int(compiler, node, source);

    case NODE_NOT:
        return compile_not(compiler, node, source);

    case NODE_NEG:
        return compile_neg(compiler, node, source);

    case NODE_ADD:
        return compile_add(compiler, node, source);

    case NODE_SUB:
        return compile_sub(compiler, node, source);

    case NODE_MUL:
        return compile_mul(compiler, node, source);

    case NODE_DIV:
        return compile_div(compiler, node, source);

    case NODE_POW:
        return compile_pow(compiler, node, source);

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
