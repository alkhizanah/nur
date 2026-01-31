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
    bool is_captured;
} Local;

typedef struct {
    uint32_t index;
    bool is_local;
} Upvalue;

typedef struct {
    Offsets breaks;
    uint32_t start;
    bool inside;
} Loop;

typedef struct Compiler {
    struct Compiler *parent;
    const char *file_path;
    const char *file_buffer;
    Ast ast;
    Vm *vm;
    Chunk *chunk;
    Loop loop;
    Local locals[UINT8_MAX];
    uint8_t locals_count;
    Upvalue upvalues[UINT8_MAX];
    uint8_t upvalues_count;
} Compiler;

bool compile_stmt(Compiler *compiler, AstNodeIdx);
bool compile_expr(Compiler *compiler, AstNodeIdx);
bool compile_block(Compiler *compiler, AstNode);
