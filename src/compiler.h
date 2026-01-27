#pragma once

#include "ast.h"
#include "vm.h"

typedef struct {
    uint32_t *items;
    size_t count;
    size_t capacity;
} Offsets;

typedef struct {
    const char *file_path;
    const char *file_buffer;
    Ast ast;
    Vm *vm;
    Chunk *chunk;
    bool in_loop;
    uint32_t loop_start;
    Offsets loop_breaks;
} Compiler;

bool compile_stmt(Compiler *compiler, AstNodeIdx);
bool compile_expr(Compiler *compiler, AstNodeIdx);
bool compile_block(Compiler *compiler, AstNode);
