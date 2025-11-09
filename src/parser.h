#pragma once

#include <stdint.h>

#include "lexer.h"

typedef enum : uint8_t {
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

typedef struct {
    uint32_t lhs;
    uint32_t rhs;
} AstNodePayload;

typedef struct {
    AstNodeTag *tags;
    AstNodePayload *payloads;
    size_t len;
    size_t capacity;
} AstNodes;

typedef struct {
    AstNodes nodes;
} Ast;

typedef struct {
    Lexer lexer;
    Ast ast;
} Parser;

void parser_parse(Parser *);
