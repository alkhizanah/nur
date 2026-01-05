#pragma once

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

#include "lexer.h"

typedef enum : uint8_t {
    // Payload: lhs is an index to a AstNodeIdx list in extra.items, and rhs is
    // the amount of nodes
    NODE_BLOCK,
    // Payload: lhs is the Range.start and rhs is the Range.end
    NODE_IDENTIFIER,
    // Payload: lhs is an index to a character list in strings.items, and rhs is
    // the amount of characters
    NODE_STRING,
    // Payload: lhs is the high bits (i >> 32) and rhs is the low bits
    // ((uint32_t)i)
    NODE_INT,
    // Payload: lhs is the high bits (bitcasted_f >> 32) and rhs is the low bits
    // ((uint32_t)bitcasted_f)
    NODE_FLOAT,
    // Payload: lhs is an index to a target node, rhs is an index to a value
    NODE_ASSIGN,
} AstNodeTag;

typedef uint32_t AstNodeIdx;

#define INVALID_NODE_IDX UINT32_MAX

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
    char *items;
    size_t len;
    size_t capacity;
} AstStrings;

typedef struct {
    uint32_t *items;
    size_t len;
    size_t capacity;
} AstExtra;

typedef struct {
    AstNodes nodes;
    AstStrings strings;
    AstExtra extra;
} Ast;

typedef struct {
    const char *file_path;
    Lexer lexer;
    Ast ast;
} AstParser;

AstNodeIdx ast_parse(AstParser *);

void ast_display(const Ast *, const char *buffer, AstNodeIdx node);
