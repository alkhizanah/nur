#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "compiler.h"
#include "parser.h"
#include "source_location.h"
#include "vm.h"

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

bool compile_block(Compiler *compiler, AstNode block) {
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

    return true;
}

static void compiler_push_constant(Compiler *compiler, Value value,
                                   uint32_t source) {
    uint16_t c = chunk_add_constant(compiler->chunk, value);
    chunk_add_byte(compiler->chunk, OP_CONST, source);
    chunk_add_byte(compiler->chunk, c >> 8, source);
    chunk_add_byte(compiler->chunk, c, source);
}

static bool compile_identifier(Compiler *compiler, AstNode node,
                               uint32_t source) {
    const char *identifier = compiler->file_buffer + node.lhs;
    size_t len = node.rhs - node.lhs;

    if (strncmp(identifier, "null", len) == 0) {
        chunk_add_byte(compiler->chunk, OP_NULL, source);
    } else if (strncmp(identifier, "true", len) == 0) {
        chunk_add_byte(compiler->chunk, OP_TRUE, source);
    } else if (strncmp(identifier, "false", len) == 0) {
        chunk_add_byte(compiler->chunk, OP_FALSE, source);
    } else {
        compiler_error(compiler, source,
                       "todo: compile global/local variables\n");

        return false;
    }

    return true;
}

static void compile_string(Compiler *compiler, AstNode node, uint32_t source) {
    const char *sv = compiler->ast.strings.items + node.lhs;
    size_t len = node.rhs;
    ObjString *s = vm_new_string(compiler->vm, node.rhs);
    memcpy(s->items, sv, len * sizeof(char));
    s->count = len;
    compiler_push_constant(compiler, OBJ_VAL(s), source);
}

static void compile_int(Compiler *compiler, AstNode node, uint32_t source) {
    uint64_t v = (uint64_t)node.lhs << 32 | node.rhs;
    compiler_push_constant(compiler, INT_VAL(v), source);
}

static void compile_float(Compiler *compiler, AstNode node, uint32_t source) {
    uint64_t v = (uint64_t)node.lhs << 32 | node.rhs;
    double f;
    memcpy(&f, &v, sizeof(double));
    compiler_push_constant(compiler, FLT_VAL(f), source);
}

static bool compile_function(Compiler *compiler, AstNode node,
                             uint32_t source) {
    uint32_t arity = 0;

    if (node.lhs != INVALID_EXTRA_IDX) {
        arity = compiler->ast.extra.items[node.lhs];

        if (arity > UINT8_MAX) {
            compiler_error(compiler, source,
                           "accepting %d paramters exceeds the limit of %d\n",
                           (int)arity, UINT8_MAX);

            return false;
        }

        // TODO: if local variables are supported this should read the parameter
        // names and do something with them, however we ignore them for now
    }

    Chunk *prev_chunk = compiler->chunk;

    CallFrame *frame = &compiler->vm->frames[compiler->vm->frame_count++];

    frame->fn = vm_new_function(compiler->vm,
                                (Chunk){
                                    .file_path = compiler->file_path,
                                    .file_content = compiler->file_buffer,
                                },
                                arity);

    frame->slots = compiler->vm->stack;

    compiler->chunk = &frame->fn->chunk;

    if (!compile_stmt(compiler, node.rhs)) {
        return false;
    }

    chunk_add_byte(compiler->chunk, OP_NULL, 0);
    chunk_add_byte(compiler->chunk, OP_RETURN, 0);

    compiler->chunk = prev_chunk;

    compiler_push_constant(compiler, OBJ_VAL(frame->fn), source);

    compiler->vm->frame_count--;

    return true;
};

static uint32_t compiler_emit_jump(Compiler *compiler, OpCode opcode,
                                   uint32_t source) {

    chunk_add_byte(compiler->chunk, opcode, source);

    chunk_add_byte(compiler->chunk, 0xff, source);
    chunk_add_byte(compiler->chunk, 0xff, source);

    return compiler->chunk->count - 2;
}

static void compiler_patch_jump(Compiler *compiler, uint32_t offset) {
    uint32_t jump = compiler->chunk->count - offset - 2;

    compiler->chunk->bytes[offset] = jump >> 8;
    compiler->chunk->bytes[offset + 1] = jump;
}

static void compiler_emit_loop(Compiler *compiler, uint32_t loop_start,
                               uint32_t source) {
    chunk_add_byte(compiler->chunk, OP_LOOP, source);

    uint32_t back_offset = compiler->chunk->count - loop_start + 2;

    chunk_add_byte(compiler->chunk, back_offset >> 8, source);
    chunk_add_byte(compiler->chunk, back_offset, source);
}

static bool compile_while_loop(Compiler *compiler, AstNode node,
                               uint32_t source) {
    uint32_t loop_start = compiler->chunk->count;

    if (!compile_expr(compiler, node.lhs)) {
        return false;
    }

    uint32_t exit_jump =
        compiler_emit_jump(compiler, OP_POP_JUMP_IF_FALSE, source);

    if (!compile_stmt(compiler, node.rhs)) {
        return false;
    }

    compiler_emit_loop(compiler, loop_start, source);

    compiler_patch_jump(compiler, exit_jump);

    return true;
}

static bool compile_conditional(Compiler *compiler, AstNode node,
                                uint32_t source) {
    AstNodeIdx true_case = compiler->ast.extra.items[node.rhs];
    AstNodeIdx false_case = compiler->ast.extra.items[node.rhs + 1];

    if (!compile_expr(compiler, node.lhs)) {
        return false;
    }

    uint32_t then_jump =
        compiler_emit_jump(compiler, OP_POP_JUMP_IF_FALSE, source);

    if (!compile_stmt(compiler, true_case)) {
        return false;
    }

    uint32_t else_jump = compiler_emit_jump(compiler, OP_JUMP, source);

    compiler_patch_jump(compiler, then_jump);

    if (false_case != INVALID_NODE_IDX) {
        if (!compile_stmt(compiler, false_case)) {
            return false;
        }
    }

    compiler_patch_jump(compiler, else_jump);

    return true;
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

static bool compile_call(Compiler *compiler, AstNode node, uint32_t source) {
    uint32_t argc = 0;

    if (node.rhs != INVALID_EXTRA_IDX) {
        uint32_t start = node.rhs + 1;

        argc = compiler->ast.extra.items[node.rhs];

        if (argc > UINT8_MAX) {
            compiler_error(compiler, source,
                           "passing %d arguments exceeds the limit of %d\n",
                           (int)argc, UINT8_MAX);

            return false;
        }

        for (uint32_t i = 0; i < argc; i++) {
            if (!compile_expr(compiler, compiler->ast.extra.items[start + i])) {
                return false;
            }
        }
    }

    if (!compile_expr(compiler, node.lhs)) {
        return false;
    }

    chunk_add_byte(compiler->chunk, OP_CALL, source);
    chunk_add_byte(compiler->chunk, argc, source);

    return true;
}

bool compile_stmt(Compiler *compiler, AstNodeIdx node_idx) {
    AstNode node = compiler->ast.nodes.items[node_idx];
    uint32_t source = compiler->ast.nodes.sources[node_idx];

    switch (node.tag) {
    case NODE_BLOCK:
        return compile_block(compiler, node);

    case NODE_RETURN:
        return compile_return(compiler, node, source);

    case NODE_WHILE:
        return compile_while_loop(compiler, node, source);

    case NODE_IF:
        return compile_conditional(compiler, node, source);

    default:
        if (!compile_expr(compiler, node_idx)) {
            return false;
        }

        chunk_add_byte(compiler->chunk, OP_POP, source);

        return true;
    }
}

bool compile_expr(Compiler *compiler, AstNodeIdx node_idx) {
    AstNode node = compiler->ast.nodes.items[node_idx];
    uint32_t source = compiler->ast.nodes.sources[node_idx];

    switch (node.tag) {
    case NODE_IDENTIFIER:
        return compile_identifier(compiler, node, source);

    case NODE_STRING:
        compile_string(compiler, node, source);
        return true;

    case NODE_INT:
        compile_int(compiler, node, source);
        return true;

    case NODE_FLOAT:
        compile_float(compiler, node, source);
        return true;

    case NODE_FUNCTION:
        return compile_function(compiler, node, source);

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

    case NODE_CALL:
        return compile_call(compiler, node, source);

    default:
        compiler_error(compiler, source, "todo: compile this\n");

        return false;
    }
}
