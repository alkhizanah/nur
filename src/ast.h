#pragma once

#include <stdint.h>

#include "lexer.h"

typedef enum : uint8_t {
    // Payload: lhs is an index in extra.items, and rhs is the amount of nodes
    NODE_BLOCK,
    // Payload: lhs is the Range.start and rhs is the Range.end
    NODE_IDENTIFIER,
    // Payload: lhs is the high bits (i >> 32) and rhs is the low bits
    // ((uint32_t)i)
    NODE_INT,
    // Payload: lhs is the high bits (bitcasted_f >> 32) and rhs is the low bits
    // ((uint32_t)bitcasted_f)
    NODE_FLOAT,
    // Payload: lhs is the Range.start and rhs is the Range.end
    NODE_STRING,
    // Payload: lhs is an index to a target node, rhs is an index to a value
    NODE_ASSIGN,
} AstNodeTag;

typedef uint32_t AstNodeIdx;

typedef struct {
    AstNodeIdx lhs;
    AstNodeIdx rhs;
} AstNodePayload;

typedef struct {
    AstNodeTag *tags;
    AstNodePayload *payloads;
    size_t len;
    size_t capacity;
} AstNodes;

typedef struct {
    uint32_t *items;
    size_t len;
    size_t capacity;
} AstExtra;

// Note: the last element in nodes is a NODE_BLOCK which is considered the root
// of the current module
typedef struct {
    AstNodes nodes;
    AstExtra extra;
} Ast;

typedef struct {
    const char *file_path;
    Lexer lexer;
    Ast ast;
} AstParser;

void ast_parse(AstParser *);
