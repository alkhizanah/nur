#pragma once

#include "ast.h"
#include "vm.h"

typedef struct {
    uint32_t *items;
    size_t count;
    size_t capacity;
} Offsets;

typedef struct {
    const char *name;
    uint32_t name_len;
} Local;

typedef struct {
    Local *items;
    size_t count;
    size_t capacity;
} Locals;

typedef struct {
    Offsets breaks;
    uint32_t start;
    bool inside;
} Loop;

typedef struct Compiler {
    const struct Compiler *parent;
    const char *file_path;
    const char *file_buffer;
    Ast ast;
    Vm *vm;
    Chunk *chunk;
    Loop loop;
    Locals locals;
} Compiler;

bool compile_stmt(Compiler *compiler, AstNodeIdx);
bool compile_expr(Compiler *compiler, AstNodeIdx);
bool compile_block(Compiler *compiler, AstNode);
