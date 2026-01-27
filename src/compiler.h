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
    Offsets *breaks;
    Offsets *continues;
} Compiler;

bool compile_stmt(Compiler *compiler, AstNodeIdx);
bool compile_expr(Compiler *compiler, AstNodeIdx);
bool compile_block(Compiler *compiler, AstNode);
