#pragma once

#include "ast.h"
#include "vm.h"

typedef struct {
    const char *file_path;
    const char *file_buffer;
    Ast ast;
    Vm *vm;
    Chunk *chunk;
    uint32_t *breaks;
    uint32_t *continues;
} Compiler;

bool compile_stmt(Compiler *compiler, AstNodeIdx);
bool compile_expr(Compiler *compiler, AstNodeIdx);
bool compile_block(Compiler *compiler, AstNode);
